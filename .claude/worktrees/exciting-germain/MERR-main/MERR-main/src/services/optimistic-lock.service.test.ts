/**
 * optimistic-lock.service.test.ts
 * ================================
 * Tests for the atomic optimistic locking helper.
 *
 * Scenarios:
 *  1. ✅ Successful update — updated_at matches, 1 trip
 *  2. ❌ Conflict detected — updated_at mismatch → PGRST116 → conflict logged
 *  3. ❌ Record deleted — PGRST116 + no server state → deleted conflict
 *  4. 💥 Network/RLS error — thrown, not swallowed
 *  5. 🔄 updateWithoutLock fallback — no version check
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { withOptimisticLock, updateWithoutLock } from './optimistic-lock.service';

// ── Mock supabase ──────────────────────────────
const mockSingle = vi.fn();
const mockSelect = vi.fn(() => ({ single: mockSingle }));
const mockEqChain = vi.fn(() => ({ select: mockSelect }));
const mockEqFirst = vi.fn(() => ({ eq: mockEqChain }));
const mockUpdate = vi.fn(() => ({ eq: mockEqFirst }));
// eslint-disable-next-line @typescript-eslint/no-explicit-any
let mockFrom = vi.fn(() => ({ update: mockUpdate })) as any;

// For conflict path: second SELECT call
const mockConflictSingle = vi.fn();
const mockConflictEq = vi.fn(() => ({ single: mockConflictSingle }));
const mockConflictSelect = vi.fn(() => ({ eq: mockConflictEq }));

// eslint-disable-next-line @typescript-eslint/no-explicit-any
import { supabase } from './supabase';
import { conflictService } from './conflict.service';

// ── Mock conflictService.detect via spyOn ──────────────────────────────
let mockDetect: ReturnType<typeof vi.fn>;

// ── Mock logger ──────────────────────────────
vi.mock('@/utils/logger', () => ({
    logger: {
        info: vi.fn(),
        warn: vi.fn(),
        error: vi.fn(),
    },
}));

describe('optimistic-lock.service', () => {
    beforeEach(() => {
        vi.restoreAllMocks();

        // Reset from to handle both UPDATE and SELECT paths
        vi.spyOn(supabase, 'from').mockImplementation((() => ({
            update: mockUpdate,
            select: mockConflictSelect,
        })) as never);
        mockFrom = vi.spyOn(supabase, 'from') as unknown as typeof mockFrom;

        // Spy on conflictService.detect
        mockDetect = vi.fn();
        vi.spyOn(conflictService, 'detect').mockImplementation(mockDetect);
    });

    describe('withOptimisticLock', () => {
        const baseOptions = {
            table: 'pickers',
            recordId: 'picker-uuid-1',
            expectedUpdatedAt: '2026-02-14T10:00:00.000Z',
            updates: { status: 'break' },
        };

        it('succeeds when updated_at matches (atomic 1-trip)', async () => {
            const updatedRow = {
                id: 'picker-uuid-1',
                status: 'break',
                updated_at: '2026-02-14T10:00:01.000Z',
            };

            mockSingle.mockResolvedValue({ data: updatedRow, error: null });

            const result = await withOptimisticLock(baseOptions);

            expect(result.success).toBe(true);
            expect(result.data).toEqual(updatedRow);
            expect(result.conflict).toBeUndefined();

            // Verify the atomic WHERE chain
            expect(mockFrom).toHaveBeenCalledWith('pickers');
            expect(mockUpdate).toHaveBeenCalledWith({ status: 'break' });
            expect(mockEqFirst).toHaveBeenCalledWith('id', 'picker-uuid-1');
            expect(mockEqChain).toHaveBeenCalledWith('updated_at', '2026-02-14T10:00:00.000Z');
        });

        it('detects conflict when updated_at does not match (PGRST116)', async () => {
            // Atomic update fails — returns PGRST116
            mockSingle.mockResolvedValue({
                data: null,
                error: { code: 'PGRST116', message: 'No rows returned' },
            });

            // Second SELECT reveals winner's state
            const serverState = {
                id: 'picker-uuid-1',
                status: 'active',
                updated_at: '2026-02-14T10:05:00.000Z',
            };
            mockConflictSingle.mockResolvedValue({ data: serverState, error: null });

            const fakeConflict = {
                id: 'conflict-1',
                table: 'pickers',
                record_id: 'picker-uuid-1',
                resolution: 'pending',
            };
            mockDetect.mockReturnValue(fakeConflict);

            const result = await withOptimisticLock(baseOptions);

            expect(result.success).toBe(false);
            expect(result.conflict).toEqual(fakeConflict);

            // Verify conflictService.detect was called with correct args
            expect(mockDetect).toHaveBeenCalledWith(
                'pickers',
                'picker-uuid-1',
                '2026-02-14T10:00:00.000Z',    // expected
                '2026-02-14T10:05:00.000Z',    // server's actual
                { status: 'break' },            // what we tried to write
                serverState                      // what the server has
            );
        });

        it('handles deleted record (PGRST116 + no server state)', async () => {
            mockSingle.mockResolvedValue({
                data: null,
                error: { code: 'PGRST116', message: 'No rows returned' },
            });

            // Record doesn't exist anymore
            mockConflictSingle.mockResolvedValue({ data: null, error: null });

            mockDetect.mockReturnValue({
                id: 'conflict-deleted',
                table: 'pickers',
                record_id: 'picker-uuid-1',
                resolution: 'pending',
            });

            const result = await withOptimisticLock(baseOptions);

            expect(result.success).toBe(false);
            expect(mockDetect).toHaveBeenCalledWith(
                'pickers',
                'picker-uuid-1',
                '2026-02-14T10:00:00.000Z',
                'DELETED',
                { status: 'break' },
                {}
            );
        });

        it('throws on network/RLS errors (non-PGRST116)', async () => {
            const networkError = { code: '42501', message: 'RLS policy violation' };
            mockSingle.mockResolvedValue({ data: null, error: networkError });

            await expect(withOptimisticLock(baseOptions)).rejects.toEqual(networkError);

            // conflictService should NOT be called for non-conflict errors
            expect(mockDetect).not.toHaveBeenCalled();
        });
    });

    describe('updateWithoutLock', () => {
        it('performs a standard update without version check', async () => {
            const updatedRow = {
                id: 'picker-uuid-1',
                status: 'inactive',
                updated_at: '2026-02-14T11:00:00.000Z',
            };

            // Reset chain for the simpler updateWithoutLock path
            const simpleSingle = vi.fn().mockResolvedValue({ data: updatedRow, error: null });
            const simpleSelect = vi.fn(() => ({ single: simpleSingle }));
            const simpleEq = vi.fn(() => ({ select: simpleSelect }));
            const simpleUpdate = vi.fn(() => ({ eq: simpleEq }));
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            mockFrom.mockImplementation((() => ({
                update: simpleUpdate,
                select: mockConflictSelect,
            })) as any);

            const result = await updateWithoutLock('pickers', 'picker-uuid-1', { status: 'inactive' });

            expect(result.success).toBe(true);
            expect(result.data).toEqual(updatedRow);
            expect(simpleUpdate).toHaveBeenCalledWith({ status: 'inactive' });
            expect(simpleEq).toHaveBeenCalledWith('id', 'picker-uuid-1');
        });

        it('throws on error', async () => {
            const dbError = { code: '23505', message: 'unique violation' };

            const simpleSingle = vi.fn().mockResolvedValue({ data: null, error: dbError });
            const simpleSelect = vi.fn(() => ({ single: simpleSingle }));
            const simpleEq = vi.fn(() => ({ select: simpleSelect }));
            const simpleUpdate = vi.fn(() => ({ eq: simpleEq }));
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            mockFrom.mockImplementation((() => ({
                update: simpleUpdate,
                select: mockConflictSelect,
            })) as any);

            await expect(updateWithoutLock('pickers', 'p1', { status: 'x' })).rejects.toEqual(dbError);
        });
    });
});
