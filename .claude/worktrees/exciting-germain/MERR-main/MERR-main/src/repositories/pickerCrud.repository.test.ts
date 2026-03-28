/**
 * PickerCrud Repository Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { supabase } from '@/services/supabase';
import { pickerCrudRepository } from './pickerCrud.repository';

function mockChain(result: { data?: unknown; error?: unknown; count?: number }) {
    const chain: Record<string, unknown> = {
        select: vi.fn(() => chain), eq: vi.fn(() => chain), neq: vi.fn(() => chain),
        in: vi.fn(() => chain), or: vi.fn(() => chain), insert: vi.fn(() => chain),
        update: vi.fn(() => chain), delete: vi.fn(() => chain), match: vi.fn(() => chain),
        single: vi.fn(() => chain),
        then: (onF?: (v: unknown) => unknown, onR?: (r: unknown) => unknown) =>
            Promise.resolve(result).then(onF, onR),
    };
    return chain;
}

describe('pickerCrudRepository', () => {
    let fromSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        vi.restoreAllMocks();
        fromSpy = vi.spyOn(supabase, 'from').mockReturnValue(mockChain({ data: [], error: null }) as never);
    });

    describe('query', () => {
        it('queries all pickers when no filters', async () => {
            const result = await pickerCrudRepository.query();
            expect(result).toEqual([]);
            expect(fromSpy).toHaveBeenCalledWith('pickers');
        });

        it('filters by teamLeaderId only', async () => {
            const result = await pickerCrudRepository.query('tl-1');
            expect(result).toEqual([]);
        });

        it('filters by orchardId only', async () => {
            const result = await pickerCrudRepository.query(undefined, 'orch-1');
            expect(result).toEqual([]);
        });

        it('uses OR logic with both filters', async () => {
            const result = await pickerCrudRepository.query('tl-1', 'orch-1');
            expect(result).toEqual([]);
        });

        it('throws on error', async () => {
            fromSpy.mockReturnValue(mockChain({ data: null, error: { message: 'fail' } }) as never);
            await expect(pickerCrudRepository.query()).rejects.toBeTruthy();
        });
    });

    describe('getTotalCount', () => {
        it('returns count', async () => {
            fromSpy.mockReturnValue(mockChain({ data: null, error: null, count: 42 }) as never);
            const count = await pickerCrudRepository.getTotalCount();
            expect(count).toBe(42);
        });
    });

    describe('bulkUpdateRow', () => {
        it('skips empty picker list', async () => {
            await pickerCrudRepository.bulkUpdateRow([], 5);
            expect(fromSpy).not.toHaveBeenCalled();
        });

        it('updates rows for given pickers', async () => {
            await expect(pickerCrudRepository.bulkUpdateRow(['p1', 'p2'], 3)).resolves.toBeUndefined();
            expect(fromSpy).toHaveBeenCalledWith('pickers');
        });

        it('throws on error', async () => {
            fromSpy.mockReturnValue(mockChain({ error: { message: 'fail' } }) as never);
            await expect(pickerCrudRepository.bulkUpdateRow(['p1'], 1)).rejects.toBeTruthy();
        });
    });

    describe('insert', () => {
        it('inserts and returns picker', async () => {
            const picker = { id: 'p1', name: 'Test' };
            fromSpy.mockReturnValue(mockChain({ data: picker, error: null }) as never);
            const result = await pickerCrudRepository.insert({ name: 'Test' });
            expect(result).toEqual(picker);
        });

        it('throws on error', async () => {
            fromSpy.mockReturnValue(mockChain({ data: null, error: { message: 'dup' } }) as never);
            await expect(pickerCrudRepository.insert({ name: 'Test' })).rejects.toBeTruthy();
        });
    });

    describe('updateById', () => {
        it('updates without throwing', async () => {
            await expect(pickerCrudRepository.updateById('p1', { name: 'New' })).resolves.toBeUndefined();
        });

        it('throws on error', async () => {
            fromSpy.mockReturnValue(mockChain({ error: { message: 'fail' } }) as never);
            await expect(pickerCrudRepository.updateById('p1', { name: 'New' })).rejects.toBeTruthy();
        });
    });

    describe('deleteById', () => {
        it('deletes without throwing', async () => {
            await expect(pickerCrudRepository.deleteById('p1')).resolves.toBeUndefined();
        });

        it('throws on error', async () => {
            fromSpy.mockReturnValue(mockChain({ error: { message: 'fail' } }) as never);
            await expect(pickerCrudRepository.deleteById('p1')).rejects.toBeTruthy();
        });
    });

    describe('findDuplicate', () => {
        it('returns duplicate picker if found', async () => {
            const dup = { id: 'p2', name: 'Dup' };
            fromSpy.mockReturnValue(mockChain({ data: dup, error: null }) as never);
            expect(await pickerCrudRepository.findDuplicate('PK-001', 'p1')).toEqual(dup);
        });

        it('returns null when no duplicate', async () => {
            fromSpy.mockReturnValue(mockChain({ data: null, error: null }) as never);
            expect(await pickerCrudRepository.findDuplicate('PK-001', 'p1')).toBeNull();
        });
    });

    describe('insertBatch', () => {
        it('inserts batch and returns ids', async () => {
            fromSpy.mockReturnValue(mockChain({ data: [{ id: 'p1' }, { id: 'p2' }], error: null }) as never);
            const result = await pickerCrudRepository.insertBatch([{ name: 'A' }, { name: 'B' }]);
            expect(result).toEqual([{ id: 'p1' }, { id: 'p2' }]);
        });

        it('throws on error', async () => {
            fromSpy.mockReturnValue(mockChain({ data: null, error: { message: 'fail' } }) as never);
            await expect(pickerCrudRepository.insertBatch([{ name: 'A' }])).rejects.toBeTruthy();
        });
    });

    describe('insertSingle', () => {
        it('inserts single picker', async () => {
            const result = await pickerCrudRepository.insertSingle({ name: 'Test' });
            expect(result).toEqual({ error: null });
        });
    });
});
