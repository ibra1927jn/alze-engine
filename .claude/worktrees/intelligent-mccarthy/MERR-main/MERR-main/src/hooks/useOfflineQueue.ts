/**
 * useOfflineQueue — Processes queued offline operations (unlinks) on reconnect
 * Extracted from Manager.tsx for reuse and testability.
 * @module hooks/useOfflineQueue
 */
import { useEffect } from 'react';
import { db } from '@/services/db';
import { userService } from '@/services/user.service';
import { logger } from '@/utils/logger';

/**
 * Processes pending offline queue items (e.g. UNLINK operations) when the
 * browser comes back online. Retries up to 3 times before giving up.
 * 
 * @param fetchGlobalData - Callback to refresh store data after processing
 */
export function useOfflineQueue(fetchGlobalData: () => void) {
    useEffect(() => {
        const processQueue = async () => {
            try {
                const pending = await db.sync_queue
                    .where('type').equals('UNLINK')
                    .toArray();
                if (pending.length === 0) return;

                logger.info(`[Offline] Connection restored — processing ${pending.length} queued unlinks`);
                for (const item of pending) {
                    try {
                        const userId = (item.payload as { userId: string }).userId;
                        await userService.unassignUserFromOrchard(userId);
                        await db.sync_queue.delete(item.id);
                        logger.info(`[Offline] Unlink processed for ${userId}`);
                    } catch (e) {
                        logger.error(`[Offline] Failed to process unlink ${item.id}:`, e);
                        // Increment retry count; give up after 3 attempts
                        if ((item.retryCount || 0) >= 3) {
                            await db.sync_queue.delete(item.id);
                            logger.warn(`[Offline] Gave up on unlink ${item.id} after 3 retries`);
                        } else {
                            await db.sync_queue.update(item.id, { retryCount: (item.retryCount || 0) + 1 });
                        }
                    }
                }
                await fetchGlobalData();
            } catch (e) {
                logger.error('[Offline] Queue processing error:', e);
            }
        };

        // Process on mount (in case we came online while the app was closed)
        if (navigator.onLine) processQueue();

        window.addEventListener('online', processQueue);
        return () => window.removeEventListener('online', processQueue);
    }, [fetchGlobalData]);
}
