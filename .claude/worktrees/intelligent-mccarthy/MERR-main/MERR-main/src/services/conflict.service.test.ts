/**
 * ============================================
 * conflict.service.test.ts
 * Regression tests for R9-Fix5: same-timestamp conflict detection
 * ============================================
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { db } from './db';
import * as nzstModule from '@/utils/nzst';
import * as uuidModule from '@/utils/uuid';

// ── Mock logger (pure utils — vi.mock works for this) ──────────────────
vi.mock('@/utils/logger', () => ({
    logger: { error: vi.fn(), info: vi.fn(), warn: vi.fn() },
}));

import { conflictService } from './conflict.service';

describe('conflictService.detect', () => {
    let mockPut: ReturnType<typeof vi.fn>;
    let mockCount: ReturnType<typeof vi.fn>;

    beforeEach(() => {
        vi.restoreAllMocks();
        // Spy on NZST and UUID
        vi.spyOn(nzstModule, 'nowNZST').mockReturnValue('2026-02-17T10:00:00');
        vi.spyOn(uuidModule, 'safeUUID').mockReturnValue('test-conflict-uuid');
        // Spy on Dexie table methods
        mockPut = vi.fn().mockResolvedValue(undefined);
        mockCount = vi.fn().mockResolvedValue(0);
        vi.spyOn(db.sync_conflicts, 'put').mockImplementation(mockPut as never);
        vi.spyOn(db.sync_conflicts, 'count').mockImplementation(mockCount as never);
        vi.spyOn(db.sync_conflicts, 'orderBy').mockReturnValue({
            limit: vi.fn().mockReturnValue({
                toArray: vi.fn().mockResolvedValue([]),
            }),
        } as never);
    });

    // ═══════════════════════════════════════════
    // R9-Fix5: Same-timestamp is treated as conflict
    // ═══════════════════════════════════════════
    describe('R9-Fix5: same-timestamp conflict detection', () => {
        it('returns NULL (no conflict) when local is strictly NEWER', async () => {
            const result = await conflictService.detect(
                'pickers',
                'record-1',
                '2026-02-17T10:00:01Z', // local: 1 second newer
                '2026-02-17T10:00:00Z', // server
                { status: 'active' },
                { status: 'inactive' }
            );

            expect(result).toBeNull();
        });

        it('returns CONFLICT when timestamps are IDENTICAL', async () => {
            // This is the critical regression test for R9-Fix5.
            // Before the fix, identical timestamps were treated as "no conflict",
            // which caused silent data loss in race conditions.
            const result = await conflictService.detect(
                'pickers',
                'record-1',
                '2026-02-17T10:00:00Z', // local: same time
                '2026-02-17T10:00:00Z', // server: same time
                { status: 'active' },
                { status: 'inactive' }
            );

            expect(result).not.toBeNull();
            expect(result!.table).toBe('pickers');
            expect(result!.record_id).toBe('record-1');
            expect(result!.resolution).toBe('pending');
            expect(result!.local_values).toEqual({ status: 'active' });
            expect(result!.server_values).toEqual({ status: 'inactive' });
        });

        it('returns CONFLICT when server is NEWER', async () => {
            const result = await conflictService.detect(
                'daily_attendance',
                'att-1',
                '2026-02-17T09:00:00Z', // local: older
                '2026-02-17T10:00:00Z', // server: newer
                { check_out_time: '17:00' },
                { check_out_time: '16:30' }
            );

            expect(result).not.toBeNull();
            expect(result!.table).toBe('daily_attendance');
            expect(result!.resolution).toBe('pending');
        });

        it('stores conflict in IndexedDB on detection', async () => {
            await conflictService.detect(
                'pickers',
                'record-2',
                '2026-02-17T10:00:00Z',
                '2026-02-17T10:00:00Z',
                { name: 'local' },
                { name: 'server' }
            );

            expect(mockPut).toHaveBeenCalledWith(
                expect.objectContaining({
                    id: 'test-conflict-uuid',
                    table: 'pickers',
                    record_id: 'record-2',
                    resolution: 'pending',
                })
            );
        });
    });
});
