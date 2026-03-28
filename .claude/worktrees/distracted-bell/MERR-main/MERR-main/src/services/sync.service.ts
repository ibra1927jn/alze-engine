import { bucketLedgerService } from './bucket-ledger.service';
import { simpleMessagingService } from './simple-messaging.service';
import { userService } from './user.service';
import { conflictService } from './conflict.service';
import { toNZST, nowNZST } from '@/utils/nzst';
import { logger } from '@/utils/logger';
import { db } from './db';
import type { QueuedSyncItem } from './db';

import { safeUUID } from '@/utils/uuid';
import { processContract, processTransport, processTimesheet, processAttendance } from './sync-processors';
import type { PendingItem, SyncPayload, AttendancePayload, ContractPayload, TransportPayload, TimesheetPayload } from './sync-processors';

export type { PendingItem };

// 🔧 R8-Fix1: Cross-tab mutex using Web Locks API
// The old `let isProcessing` only guarded within ONE tab.
// navigator.locks works across all tabs in the same origin.
let isProcessing = false; // Fallback for browsers without Web Locks

export const syncService = {

    // 1. Add to Queue (Persist to IndexedDB immediately)
    // updated_at: pass the record's current updated_at to enable optimistic locking on UPDATEs
    async addToQueue(type: PendingItem['type'], payload: SyncPayload, updated_at?: string) {
        const newItem: QueuedSyncItem = {
            id: safeUUID(),
            type,
            payload: payload as Record<string, unknown>,
            timestamp: Date.now(),
            retryCount: 0,
            ...(updated_at ? { updated_at } : {}),
        };

        await db.sync_queue.put(newItem);

        // Try to sync immediately if online
        if (navigator.onLine) {
            this.processQueue();
        }

        return newItem.id;
    },

    // 2. Get Queue (from IndexedDB)
    async getQueue(): Promise<QueuedSyncItem[]> {
        try {
            return await db.sync_queue.toArray();
        } catch (e) {
            logger.error("SyncService: Failed to read queue from IndexedDB", e);
            return [];
        }
    },

    // 3. Process Queue — with cross-tab lock
    // 🔧 R8-Fix1: Uses Web Locks API for cross-tab mutex
    async processQueue() {
        if (!navigator.onLine) return;

        // Web Locks API: cross-tab mutex at browser/OS level
        if (typeof navigator !== 'undefined' && 'locks' in navigator) {
            await navigator.locks.request('harvest_sync_lock', { ifAvailable: true }, async (lock) => {
                if (!lock) {
                    logger.info('[SyncService] Another tab is already syncing. Skipping.');
                    return;
                }
                await this._doProcessQueue();
            });
        } else {
            // Fallback for browsers without Web Locks (Safari private mode, etc.)
            if (isProcessing) return;
            isProcessing = true;
            try {
                await this._doProcessQueue();
            } finally {
                isProcessing = false;
            }
        }
    },

    // Internal: actual queue processing logic (called under lock)
    async _doProcessQueue() {
        const queue = await this.getQueue();
        if (queue.length === 0) return;

        const processedIds: string[] = [];

        for (const item of queue) {
            try {
                switch (item.type) {
                    case 'SCAN':
                        await bucketLedgerService.recordBucket({
                            ...(item.payload as unknown as Record<string, unknown>),
                            scanned_at: toNZST(new Date(item.timestamp))
                        } as Parameters<typeof bucketLedgerService.recordBucket>[0]);
                        break;

                    case 'ATTENDANCE':
                        await processAttendance(
                            item.payload as unknown as AttendancePayload,
                            item.updated_at
                        );
                        break;

                    case 'ASSIGNMENT':
                        await userService.assignUserToOrchard(
                            item.payload.userId as string,
                            item.payload.orchardId as string
                        );
                        break;

                    case 'MESSAGE':
                        await simpleMessagingService.sendMessage(
                            item.payload.receiverId as string,
                            item.payload.content as string,
                            (item.payload.type as string) || 'direct'
                        );
                        break;

                    // Delegated to extracted Strategy processors
                    case 'CONTRACT':
                        await processContract(item.payload as unknown as ContractPayload, item.updated_at);
                        break;

                    case 'TRANSPORT':
                        await processTransport(item.payload as unknown as TransportPayload, item.updated_at);
                        break;

                    case 'TIMESHEET':
                        await processTimesheet(item.payload as unknown as TimesheetPayload, item.updated_at);
                        break;

                    default:
                        logger.warn(`[SyncService] Unknown item type: ${item.type}`);
                        break;
                }

                // If we reach here, sync was successful — mark for removal
                processedIds.push(item.id);

            } catch (e) {
                const errorCategory = this.categorizeError(e);
                logger.error(`[SyncService] Failed to sync item ${item.id} (${errorCategory})`, e);

                // 🔧 U2: If network is down, abort the entire loop immediately
                if (errorCategory === 'network') {
                    logger.warn('[SyncService] Network down — aborting queue processing');
                    await db.sync_queue.update(item.id, { retryCount: item.retryCount + 1 });
                    break;
                }

                const newRetryCount = item.retryCount + 1;
                const maxRetries = errorCategory === 'validation' ? 5 : 50;

                if (newRetryCount < maxRetries) {
                    await db.sync_queue.update(item.id, { retryCount: newRetryCount });
                } else {
                    logger.error(`[SyncService] Giving up on item ${item.id} after ${newRetryCount} retries (${errorCategory}).`);
                    await conflictService.detect(
                        item.type.toLowerCase(),
                        item.id,
                        toNZST(new Date(item.timestamp)),
                        nowNZST(),
                        item.payload,
                        { error: 'max_retries_exceeded', category: errorCategory, retryCount: newRetryCount }
                    );
                    // Persist to DLQ in IndexedDB for admin review
                    try {
                        await db.dead_letter_queue.put({
                            id: item.id,
                            type: item.type,
                            payload: item.payload,
                            timestamp: item.timestamp,
                            retryCount: newRetryCount,
                            failureReason: `${errorCategory}: max retries exceeded`,
                            errorCode: e instanceof Error ? e.message : String(e),
                            movedAt: Date.now(),
                        });
                        // 🔧 V28: Only delete from sync_queue if DLQ insert succeeded
                        processedIds.push(item.id);
                    } catch (dlqError) {
                        // 🔧 V28: Keep item in sync_queue — better stuck than lost
                        logger.error('[SyncService] CRITICAL: DLQ insert failed. Item preserved in sync_queue:', dlqError);
                    }
                }
            }
        }

        // Bulk-delete processed items from IndexedDB
        if (processedIds.length > 0) {
            await db.sync_queue.bulkDelete(processedIds);
        }

        // Track last successful sync time
        if (processedIds.length > 0) {
            await this.setLastSyncTime();
        }
    },

    // 4. Get Pending Count (For UI Badges)
    async getPendingCount(): Promise<number> {
        return await db.sync_queue.count();
    },

    // 5. Last Sync Timestamp (from IndexedDB)
    async getLastSyncTime(): Promise<number | null> {
        try {
            const meta = await db.sync_meta.get('lastSync');
            return meta ? meta.value : null;
        } catch (e) {
            logger.warn('[SyncService] Failed to read last sync time:', e);
            return null;
        }
    },

    async setLastSyncTime() {
        try {
            await db.sync_meta.put({ id: 'lastSync', value: Date.now() });
        } catch (e) {
            logger.warn('[SyncService] Failed to save last sync time:', e);
        }
    },

    // 6. Get Max Retry Count (for UI display)
    async getMaxRetryCount(): Promise<number> {
        const queue = await this.getQueue();
        if (queue.length === 0) return 0;
        return Math.max(...queue.map(item => item.retryCount));
    },

    /**
     * 7. Queue Summary — Per-type breakdown with retry stats
     */
    async getQueueSummary(): Promise<{
        total: number;
        byType: Record<string, number>;
        maxRetry: number;
        oldestTimestamp: number | null;
        lastSync: number | null;
    }> {
        const queue = await this.getQueue();
        const byType: Record<string, number> = {};

        for (const item of queue) {
            byType[item.type] = (byType[item.type] || 0) + 1;
        }

        return {
            total: queue.length,
            byType,
            maxRetry: queue.length > 0 ? Math.max(...queue.map(i => i.retryCount)) : 0,
            oldestTimestamp: queue.length > 0 ? Math.min(...queue.map(i => i.timestamp)) : null,
            lastSync: await this.getLastSyncTime(),
        };
    },

    /**
     * 8. Categorize Error — network | server | validation | unknown
     */
    categorizeError(error: unknown): 'network' | 'server' | 'validation' | 'unknown' {
        if (!navigator.onLine) return 'network';

        if (error instanceof Error) {
            const msg = error.message.toLowerCase();
            if (msg.includes('fetch') || msg.includes('network') || msg.includes('timeout') || msg.includes('aborted')) {
                return 'network';
            }
            if (msg.includes('500') || msg.includes('502') || msg.includes('503') || msg.includes('429')) {
                return 'server';
            }
            if (msg.includes('23') || msg.includes('constraint') || msg.includes('violat') || msg.includes('unique') || msg.includes('foreign key') || msg.includes('conflict') || msg.includes('optimistic lock')) {
                return 'validation';
            }
        }

        // Supabase error objects
        if (typeof error === 'object' && error !== null && 'code' in error) {
            const code = String((error as Record<string, unknown>).code);
            if (code.startsWith('23')) return 'validation';
            if (code === 'PGRST') return 'server';
        }

        return 'unknown';
    },
};

// Auto-start processing when online — IMMEDIATE sync (no jitter)
// 🔧 Fix 10: Removed 30s jitter. Users lock phones after reconnecting,
// killing the delayed sync. Data must ship immediately on reconnect.
// The mutex (Fix 8) prevents thundering herd within a single device.
if (typeof window !== 'undefined') {
    window.addEventListener('online', () => {
        syncService.processQueue();
    });

    // Also try on load (staggered by natural page load times)
    setTimeout(() => syncService.processQueue(), 5000);
}