/**
 * timesheet.processor.test.ts — Unit tests
 *
 * Uses vi.spyOn on actual services for processTimesheet.
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { supabase } from '../supabase';

import { processTimesheet } from './timesheet.processor';

describe('processTimesheet', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
    });

    describe('approve with optimistic lock', () => {
        it('uses withOptimisticLock when expectedUpdatedAt present', async () => {
            const lockSpy = vi.spyOn(
                await import('../optimistic-lock.service'),
                'withOptimisticLock'
            ).mockResolvedValue({ success: true } as never);

            await processTimesheet(
                { action: 'approve', attendanceId: 'att-1', verifiedBy: 'mgr-1' },
                '2026-03-04T10:00:00Z'
            );

            expect(lockSpy).toHaveBeenCalledWith({
                table: 'daily_attendance',
                recordId: 'att-1',
                expectedUpdatedAt: '2026-03-04T10:00:00Z',
                updates: { verified_by: 'mgr-1' },
            });
        });

        it('throws on lock conflict', async () => {
            vi.spyOn(
                await import('../optimistic-lock.service'),
                'withOptimisticLock'
            ).mockResolvedValue({ success: false } as never);

            await expect(
                processTimesheet(
                    { action: 'approve', attendanceId: 'att-1', verifiedBy: 'mgr-1' },
                    '2026-03-04T10:00:00Z'
                )
            ).rejects.toThrow('Optimistic lock conflict');
        });
    });

    describe('approve without optimistic lock', () => {
        it('uses raw supabase update', async () => {
            const mockEq = vi.fn().mockResolvedValue({ error: null });
            const mockUpdate = vi.fn().mockReturnValue({ eq: mockEq });
            vi.spyOn(supabase, 'from').mockReturnValue({
                update: mockUpdate,
            } as never);

            await processTimesheet(
                { action: 'approve', attendanceId: 'att-1', verifiedBy: 'mgr-1' }
            );

            expect(supabase.from).toHaveBeenCalledWith('daily_attendance');
            expect(mockUpdate).toHaveBeenCalledWith({ verified_by: 'mgr-1' });
            expect(mockEq).toHaveBeenCalledWith('id', 'att-1');
        });

        it('throws on supabase error', async () => {
            const mockEq = vi.fn().mockResolvedValue({ error: { message: 'RLS' } });
            vi.spyOn(supabase, 'from').mockReturnValue({
                update: vi.fn().mockReturnValue({ eq: mockEq }),
            } as never);

            await expect(
                processTimesheet(
                    { action: 'approve', attendanceId: 'att-1', verifiedBy: 'mgr-1' }
                )
            ).rejects.toBeDefined();
        });
    });
});
