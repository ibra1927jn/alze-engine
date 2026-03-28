/**
 * BucketEvents Repository Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { supabase } from '@/services/supabase';
import { bucketEventsRepository } from './bucketEvents.repository';

function mockChain(result: { data?: unknown; error?: unknown }) {
    const chain: Record<string, unknown> = {
        select: vi.fn(() => chain), eq: vi.fn(() => chain), in: vi.fn(() => chain),
        insert: vi.fn(() => chain), update: vi.fn(() => chain),
        then: (onF?: (v: unknown) => unknown, onR?: (r: unknown) => unknown) =>
            Promise.resolve(result).then(onF, onR),
    };
    return chain;
}

describe('bucketEventsRepository', () => {
    let fromSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        vi.restoreAllMocks();
        fromSpy = vi.spyOn(supabase, 'from').mockReturnValue(mockChain({ data: [], error: null }) as never);
    });

    describe('insertBatch', () => {
        it('inserts rows and returns no error on success', async () => {
            const rows = [{ picker_id: 'p1' }, { picker_id: 'p2' }];
            const result = await bucketEventsRepository.insertBatch(rows);
            expect(result).toEqual({ error: null });
            expect(fromSpy).toHaveBeenCalledWith('bucket_events');
        });

        it('returns error on failure', async () => {
            fromSpy.mockReturnValue(mockChain({ error: { message: 'insert failed' } }) as never);
            const result = await bucketEventsRepository.insertBatch([{ picker_id: 'p1' }]);
            expect(result.error).toBeTruthy();
        });
    });

    describe('insertSingle', () => {
        it('inserts single row on success', async () => {
            const result = await bucketEventsRepository.insertSingle({ picker_id: 'p1' });
            expect(result).toEqual({ error: null });
            expect(fromSpy).toHaveBeenCalledWith('bucket_events');
        });

        it('returns error on failure', async () => {
            fromSpy.mockReturnValue(mockChain({ error: { message: 'fail' } }) as never);
            const result = await bucketEventsRepository.insertSingle({ picker_id: 'p1' });
            expect(result.error).toBeTruthy();
        });
    });
});
