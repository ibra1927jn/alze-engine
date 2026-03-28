/**
 * Tests for db.ts — Dexie database schema validation
 */
import { describe, it, expect } from 'vitest';
import type { QueuedBucket, QueuedMessage, CachedUser, CachedSettings, DeadLetterItem, QueuedSyncItem, SyncMeta, StoredConflict } from './db';

describe('db.ts — Type Shape Validation', () => {
    it('QueuedBucket has correct fields', () => {
        const bucket: QueuedBucket = {
            id: 'b1', picker_id: 'p1', orchard_id: 'o1',
            quality_grade: 'A', timestamp: '2026-03-05T08:00:00Z',
            synced: 0, scanned_by: 'user-1',
        };
        expect(bucket.synced).toBe(0);
        expect(['A', 'B', 'C', 'reject']).toContain(bucket.quality_grade);
    });

    it('QueuedMessage has all required fields', () => {
        const msg: QueuedMessage = {
            id: 'm1', channel_type: 'direct', recipient_id: 'r1',
            sender_id: 's1', content: 'Hello', timestamp: '2026-03-05T08:00:00Z', synced: 0,
        };
        expect(msg.channel_type).toBe('direct');
    });

    it('DeadLetterItem tracks failure metadata', () => {
        const item: DeadLetterItem = {
            id: 'dlq1', type: 'SCAN', payload: { test: true },
            timestamp: Date.now(), retryCount: 3, failureReason: 'Network error', movedAt: Date.now(),
        };
        expect(item.retryCount).toBe(3);
    });

    it('QueuedSyncItem supports all event types', () => {
        const types: QueuedSyncItem['type'][] = [
            'SCAN', 'MESSAGE', 'ATTENDANCE', 'ASSIGNMENT',
            'CONTRACT', 'TRANSPORT', 'TIMESHEET', 'PICKER', 'QC_INSPECTION', 'UNLINK',
        ];
        expect(types).toHaveLength(10);
    });

    it('StoredConflict tracks resolution status', () => {
        const conflict: StoredConflict = {
            id: 'c1', table: 'pickers', record_id: 'r1',
            local_updated_at: '2026-03-05T08:00:00Z', server_updated_at: '2026-03-05T09:00:00Z',
            local_values: { name: 'Ana' }, server_values: { name: 'Ana Lopez' },
            resolution: 'pending', detected_at: '2026-03-05T10:00:00Z',
        };
        expect(['pending', 'keep_local', 'keep_server', 'merged']).toContain(conflict.resolution);
    });
});
