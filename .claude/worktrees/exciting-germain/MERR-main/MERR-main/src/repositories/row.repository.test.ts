/**
 * Row Repository Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { supabase } from '@/services/supabase';
import { rowRepository } from './row.repository';

function mockChain(result: { data?: unknown; error?: unknown }) {
    const chain: Record<string, unknown> = {
        select: vi.fn(() => chain), eq: vi.fn(() => chain), in: vi.fn(() => chain),
        update: vi.fn(() => chain), order: vi.fn(() => chain),
        then: (onF?: (v: unknown) => unknown, onR?: (r: unknown) => unknown) =>
            Promise.resolve(result).then(onF, onR),
    };
    return chain;
}

describe('rowRepository', () => {
    let fromSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        vi.restoreAllMocks();
        fromSpy = vi.spyOn(supabase, 'from').mockReturnValue(mockChain({ data: [], error: null }) as never);
    });

    describe('updatePickerRows', () => {
        it('updates picker rows without error', async () => {
            const result = await rowRepository.updatePickerRows(['p1', 'p2'], 5);
            expect(result).toEqual({ error: null });
            expect(fromSpy).toHaveBeenCalledWith('pickers');
        });

        it('returns error on failure', async () => {
            fromSpy.mockReturnValue(mockChain({ error: { message: 'fail' } }) as never);
            const result = await rowRepository.updatePickerRows(['p1'], 3);
            expect(result.error).toBeTruthy();
        });
    });

    describe('updateProgress', () => {
        it('updates progress without throwing', async () => {
            await expect(rowRepository.updateProgress('row-1', 50)).resolves.toBeUndefined();
            expect(fromSpy).toHaveBeenCalledWith('row_assignments');
        });

        it('throws on error', async () => {
            fromSpy.mockReturnValue(mockChain({ error: { message: 'fail' } }) as never);
            await expect(rowRepository.updateProgress('row-1', 50)).rejects.toBeTruthy();
        });
    });

    describe('completeRow', () => {
        it('completes row without throwing', async () => {
            await expect(rowRepository.completeRow('row-1')).resolves.toBeUndefined();
            expect(fromSpy).toHaveBeenCalledWith('row_assignments');
        });

        it('throws on error', async () => {
            fromSpy.mockReturnValue(mockChain({ error: { message: 'fail' } }) as never);
            await expect(rowRepository.completeRow('row-1')).rejects.toBeTruthy();
        });
    });
});
