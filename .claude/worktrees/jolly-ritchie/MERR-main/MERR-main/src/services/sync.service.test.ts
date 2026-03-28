// =============================================
// SYNC SERVICE TESTS (Dexie/IndexedDB)
// =============================================
import { describe, it, expect, beforeEach } from 'vitest';
import 'fake-indexeddb/auto';
import { syncService } from './sync.service';
import { db } from './db';

// Valid payloads for each type
const SCAN_PAYLOAD = {
    picker_id: 'pk-001',
    orchard_id: 'orchard-001',
    quality_grade: 'A' as const,
    timestamp: '2026-02-12T10:00:00+13:00',
};

const MESSAGE_PAYLOAD = {
    channel_type: 'direct' as const,
    recipient_id: 'user-002',
    sender_id: 'user-001',
    content: 'Hello!',
    timestamp: '2026-02-12T10:00:00+13:00',
};

const ATTENDANCE_PAYLOAD = {
    picker_id: 'pk-002',
    orchard_id: 'orchard-001',
    check_in_time: '2026-02-12T08:00:00+13:00',
};

describe('Sync Service (Dexie)', () => {
    beforeEach(async () => {
        // Clear all IndexedDB tables before each test
        await db.sync_queue.clear();
        await db.sync_meta.clear();
    });

    // =============================================
    // QUEUE OPERATIONS
    // =============================================
    describe('addToQueue', () => {
        it('should add an item to the queue in IndexedDB', async () => {
            await syncService.addToQueue('SCAN', SCAN_PAYLOAD);
            expect(await syncService.getPendingCount()).toBe(1);
        });

        it('should add multiple items to the queue', async () => {
            await syncService.addToQueue('SCAN', SCAN_PAYLOAD);
            await syncService.addToQueue('MESSAGE', MESSAGE_PAYLOAD);
            await syncService.addToQueue('ATTENDANCE', ATTENDANCE_PAYLOAD);
            expect(await syncService.getPendingCount()).toBe(3);
        });

        it('should assign unique IDs to each item', async () => {
            await syncService.addToQueue('SCAN', SCAN_PAYLOAD);
            await syncService.addToQueue('SCAN', { ...SCAN_PAYLOAD, picker_id: 'pk-002' });
            const queue = await db.sync_queue.toArray();
            expect(queue[0].id).not.toBe(queue[1].id);
        });

        it('should set retryCount to 0 for new items', async () => {
            await syncService.addToQueue('SCAN', SCAN_PAYLOAD);
            const queue = await db.sync_queue.toArray();
            expect(queue[0].retryCount).toBe(0);
        });

        it('should include timestamp on each item', async () => {
            await syncService.addToQueue('SCAN', SCAN_PAYLOAD);
            const queue = await db.sync_queue.toArray();
            expect(queue[0].timestamp).toBeDefined();
            expect(typeof queue[0].timestamp).toBe('number');
        });
    });

    // =============================================
    // PENDING COUNT
    // =============================================
    describe('getPendingCount', () => {
        it('should return 0 when queue is empty', async () => {
            expect(await syncService.getPendingCount()).toBe(0);
        });

        it('should return correct count after adding items', async () => {
            await syncService.addToQueue('SCAN', SCAN_PAYLOAD);
            await syncService.addToQueue('SCAN', { ...SCAN_PAYLOAD, picker_id: 'pk-002' });
            await syncService.addToQueue('SCAN', { ...SCAN_PAYLOAD, picker_id: 'pk-003' });
            expect(await syncService.getPendingCount()).toBe(3);
        });
    });

    // =============================================
    // LAST SYNC TIME
    // =============================================
    describe('getLastSyncTime / setLastSyncTime', () => {
        it('should return null when no sync has occurred', async () => {
            expect(await syncService.getLastSyncTime()).toBeNull();
        });

        it('should store and retrieve last sync time', async () => {
            await syncService.setLastSyncTime();
            const time = await syncService.getLastSyncTime();
            expect(time).not.toBeNull();
            expect(typeof time).toBe('number');
            // Should be within last second
            expect(Date.now() - time!).toBeLessThan(1000);
        });

        it('should update on subsequent calls', async () => {
            await syncService.setLastSyncTime();
            const first = await syncService.getLastSyncTime();
            await syncService.setLastSyncTime();
            const second = await syncService.getLastSyncTime();
            expect(second).toBeGreaterThanOrEqual(first!);
        });
    });

    // =============================================
    // MAX RETRY COUNT
    // =============================================
    describe('getMaxRetryCount', () => {
        it('should return 0 when queue is empty', async () => {
            expect(await syncService.getMaxRetryCount()).toBe(0);
        });

        it('should return the highest retry count', async () => {
            // Insert items directly into IndexedDB with different retryCount values
            await db.sync_queue.bulkPut([
                { id: '1', type: 'SCAN', payload: SCAN_PAYLOAD as unknown as Record<string, unknown>, retryCount: 3, timestamp: Date.now() },
                { id: '2', type: 'SCAN', payload: SCAN_PAYLOAD as unknown as Record<string, unknown>, retryCount: 10, timestamp: Date.now() },
                { id: '3', type: 'SCAN', payload: SCAN_PAYLOAD as unknown as Record<string, unknown>, retryCount: 1, timestamp: Date.now() },
            ]);
            expect(await syncService.getMaxRetryCount()).toBe(10);
        });
    });

    // =============================================
    // QUEUE TYPE FILTERING
    // =============================================
    describe('queue item types', () => {
        it('should store type correctly for SCAN items', async () => {
            await syncService.addToQueue('SCAN', SCAN_PAYLOAD);
            const queue = await db.sync_queue.toArray();
            expect(queue[0].type).toBe('SCAN');
        });

        it('should store type correctly for MESSAGE items', async () => {
            await syncService.addToQueue('MESSAGE', MESSAGE_PAYLOAD);
            const queue = await db.sync_queue.toArray();
            expect(queue[0].type).toBe('MESSAGE');
        });

        it('should store type correctly for ATTENDANCE items', async () => {
            await syncService.addToQueue('ATTENDANCE', ATTENDANCE_PAYLOAD);
            const queue = await db.sync_queue.toArray();
            expect(queue[0].type).toBe('ATTENDANCE');
        });
    });

    // =============================================
    // QUEUE SUMMARY (NEW)
    // =============================================
    describe('getQueueSummary', () => {
        it('should return empty summary when queue is empty', async () => {
            const summary = await syncService.getQueueSummary();
            expect(summary.total).toBe(0);
            expect(summary.maxRetry).toBe(0);
            expect(summary.oldestTimestamp).toBeNull();
        });

        it('should return correct breakdown by type', async () => {
            await syncService.addToQueue('SCAN', SCAN_PAYLOAD);
            await syncService.addToQueue('SCAN', { ...SCAN_PAYLOAD, picker_id: 'pk-002' });
            await syncService.addToQueue('MESSAGE', MESSAGE_PAYLOAD);
            const summary = await syncService.getQueueSummary();
            expect(summary.total).toBe(3);
            expect(summary.byType['SCAN']).toBe(2);
            expect(summary.byType['MESSAGE']).toBe(1);
        });
    });
});
