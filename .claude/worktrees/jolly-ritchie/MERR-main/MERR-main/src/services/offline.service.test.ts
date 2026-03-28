// =============================================
// OFFLINE SERVICE TESTS (Dexie/IndexedDB)
// =============================================
import { describe, it, expect, beforeEach } from 'vitest';
import 'fake-indexeddb/auto';
import { offlineService } from './offline.service';
import { db } from './db';

describe('Offline Service', () => {
    beforeEach(async () => {
        // Clear all Dexie tables before each test
        await db.bucket_queue.clear();
    });

    // =============================================
    // QUEUE BUCKET
    // =============================================
    describe('queueBucket', () => {
        it('should add a bucket to the queue', async () => {
            await offlineService.queueBucket({
                id: 'bucket-001',
                picker_id: 'pk-001',
                orchard_id: 'orchard-001',
                quality_grade: 'A',
                timestamp: '2026-02-12T10:00:00+13:00',
                scanned_by: 'test-user',
            });

            const pending = await offlineService.getPendingBuckets();
            expect(pending).toHaveLength(1);
            expect(pending[0].id).toBe('bucket-001');
            expect(pending[0].synced).toBe(0);
        });

        it('should queue multiple buckets', async () => {
            await offlineService.queueBucket({
                id: 'b-001',
                picker_id: 'pk-001',
                orchard_id: 'o-001',
                quality_grade: 'A',
                timestamp: '2026-02-12T10:00:00+13:00',
                scanned_by: 'test-user',
            });
            await offlineService.queueBucket({
                id: 'b-002',
                picker_id: 'pk-002',
                orchard_id: 'o-001',
                quality_grade: 'B',
                timestamp: '2026-02-12T10:01:00+13:00',
                scanned_by: 'test-user',
            });

            const count = await offlineService.getPendingCount();
            expect(count).toBe(2);
        });

        it('should set synced flag to 0 (pending)', async () => {
            await offlineService.queueBucket({
                id: 'b-003',
                picker_id: 'pk-001',
                orchard_id: 'o-001',
                quality_grade: 'A',
                timestamp: '2026-02-12T10:00:00+13:00',
                scanned_by: 'test-user',
            });

            const bucket = await db.bucket_queue.get('b-003');
            expect(bucket?.synced).toBe(0);
        });
    });

    // =============================================
    // MARK AS SYNCED
    // =============================================
    describe('markAsSynced', () => {
        it('should mark a bucket as synced', async () => {
            await offlineService.queueBucket({
                id: 'b-sync-001',
                picker_id: 'pk-001',
                orchard_id: 'o-001',
                quality_grade: 'A',
                timestamp: '2026-02-12T10:00:00+13:00',
                scanned_by: 'test-user',
            });

            await offlineService.markAsSynced('b-sync-001');

            const bucket = await db.bucket_queue.get('b-sync-001');
            expect(bucket?.synced).toBe(1);
        });

        it('should not count synced buckets as pending', async () => {
            await offlineService.queueBucket({
                id: 'b-s1',
                picker_id: 'pk-001',
                orchard_id: 'o-001',
                quality_grade: 'A',
                timestamp: '2026-02-12T10:00:00+13:00',
                scanned_by: 'test-user',
            });
            await offlineService.queueBucket({
                id: 'b-s2',
                picker_id: 'pk-002',
                orchard_id: 'o-001',
                quality_grade: 'B',
                timestamp: '2026-02-12T10:01:00+13:00',
                scanned_by: 'test-user',
            });

            await offlineService.markAsSynced('b-s1');

            const count = await offlineService.getPendingCount();
            expect(count).toBe(1);
        });
    });

    // =============================================
    // PENDING COUNT
    // =============================================
    describe('getPendingCount', () => {
        it('should return 0 when empty', async () => {
            const count = await offlineService.getPendingCount();
            expect(count).toBe(0);
        });

        it('should count only unsynced items', async () => {
            await offlineService.queueBucket({
                id: 'c1', picker_id: 'p1', orchard_id: 'o1', quality_grade: 'A', timestamp: '2026-02-12T10:00:00+13:00', scanned_by: 'test-user',
            });
            await offlineService.queueBucket({
                id: 'c2', picker_id: 'p2', orchard_id: 'o1', quality_grade: 'B', timestamp: '2026-02-12T10:01:00+13:00', scanned_by: 'test-user',
            });
            await offlineService.queueBucket({
                id: 'c3', picker_id: 'p3', orchard_id: 'o1', quality_grade: 'A', timestamp: '2026-02-12T10:02:00+13:00', scanned_by: 'test-user',
            });

            await offlineService.markAsSynced('c2');

            const count = await offlineService.getPendingCount();
            expect(count).toBe(2);
        });
    });

    // =============================================
    // QUEUE STATS
    // =============================================
    describe('getQueueStats', () => {
        it('should return all zeros when empty', async () => {
            const stats = await offlineService.getQueueStats();
            expect(stats.pending).toBe(0);
            expect(stats.synced).toBe(0);
            expect(stats.errors).toBe(0);
            expect(stats.oldestPending).toBeNull();
        });

        it('should correctly categorize items', async () => {
            // Add 3 buckets
            await offlineService.queueBucket({
                id: 'q1', picker_id: 'p1', orchard_id: 'o1', quality_grade: 'A', timestamp: '2026-02-12T10:00:00+13:00', scanned_by: 'test-user',
            });
            await offlineService.queueBucket({
                id: 'q2', picker_id: 'p2', orchard_id: 'o1', quality_grade: 'B', timestamp: '2026-02-12T10:05:00+13:00', scanned_by: 'test-user',
            });
            await offlineService.queueBucket({
                id: 'q3', picker_id: 'p3', orchard_id: 'o1', quality_grade: 'A', timestamp: '2026-02-12T10:10:00+13:00', scanned_by: 'test-user',
            });

            // Sync one, error one
            await offlineService.markAsSynced('q2');
            await db.bucket_queue.update('q3', { synced: -1 });

            const stats = await offlineService.getQueueStats();
            expect(stats.pending).toBe(1);
            expect(stats.synced).toBe(1);
            expect(stats.errors).toBe(1);
        });

        it('should return oldest pending timestamp', async () => {
            await offlineService.queueBucket({
                id: 'old1', picker_id: 'p1', orchard_id: 'o1', quality_grade: 'A', timestamp: '2026-02-12T08:00:00+13:00', scanned_by: 'test-user',
            });
            await offlineService.queueBucket({
                id: 'old2', picker_id: 'p2', orchard_id: 'o1', quality_grade: 'B', timestamp: '2026-02-12T10:00:00+13:00', scanned_by: 'test-user',
            });

            const stats = await offlineService.getQueueStats();
            expect(stats.oldestPending).toBe('2026-02-12T08:00:00+13:00');
        });
    });

    // =============================================
    // CONFLICT COUNT (LEGACY)
    // =============================================
    describe('getConflictCount', () => {
        it('should always return 0 (legacy stub)', async () => {
            const count = await offlineService.getConflictCount();
            expect(count).toBe(0);
        });
    });
});
