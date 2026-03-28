/**
 * ============================================
 * picker.service.test.ts
 * Tests for picker/workforce management
 * ============================================
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';

// ── Mock Supabase ──────────────────────────
function createChainMock(data: unknown = null, error: unknown = null) {
    const result = { data, error, count: Array.isArray(data) ? data.length : 0 };
    const chain: Record<string, unknown> = {};
    chain.select = vi.fn().mockReturnValue(chain);
    chain.insert = vi.fn().mockReturnValue(chain);
    chain.update = vi.fn().mockReturnValue(chain);
    chain.delete = vi.fn().mockReturnValue(chain);
    chain.eq = vi.fn().mockReturnValue(chain);
    chain.neq = vi.fn().mockReturnValue(chain);
    chain.in = vi.fn().mockReturnValue(chain);
    chain.is = vi.fn().mockReturnValue(chain);
    chain.or = vi.fn().mockReturnValue(chain);
    chain.match = vi.fn().mockReturnValue(chain);
    chain.order = vi.fn().mockReturnValue(chain);
    chain.limit = vi.fn().mockReturnValue(chain);
    chain.maybeSingle = vi.fn().mockResolvedValue(result);
    chain.single = vi.fn().mockResolvedValue(result);
    chain.then = vi.fn().mockImplementation(
        (resolve: (v: typeof result) => void) => Promise.resolve(resolve(result))
    );
    return chain;
}

import { supabase } from './supabase';
import { pickerService } from './picker.service';

let mockFrom: ReturnType<typeof vi.spyOn>;

describe('pickerService', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
        mockFrom = vi.spyOn(supabase, 'from') as unknown as ReturnType<typeof vi.spyOn>;
    });

    // ═══════════════════════════════
    // getPickersByTeam
    // ═══════════════════════════════
    describe('getPickersByTeam', () => {
        const mockPickers = [
            {
                id: 'uuid-1', picker_id: 'PK001', name: 'Alice', role: 'picker',
                status: 'active', orchard_id: 'orchard-1', team_leader_id: 'tl-1',
                current_row: 3, total_buckets_today: 12, safety_verified: true,
            },
            {
                id: 'uuid-2', picker_id: 'PK002', name: 'Bob', role: 'picker',
                status: 'active', orchard_id: 'orchard-1', team_leader_id: 'tl-1',
                current_row: 5, total_buckets_today: 8, safety_verified: false,
            },
        ];

        it('fetches pickers for a team leader', async () => {
            const chain = createChainMock(mockPickers);
            mockFrom.mockReturnValue(chain);

            const result = await pickerService.getPickersByTeam('tl-1');

            expect(mockFrom).toHaveBeenCalledWith('pickers');
            expect(result).toHaveLength(2);
            expect(result[0].name).toBe('Alice');
        });

        it('fetches pickers by orchard only', async () => {
            const chain = createChainMock(mockPickers);
            mockFrom.mockReturnValue(chain);

            const result = await pickerService.getPickersByTeam(undefined, 'orchard-1');

            expect(result).toHaveLength(2);
        });

        it('returns empty array on no data', async () => {
            const chain = createChainMock(null);
            mockFrom.mockReturnValue(chain);

            const result = await pickerService.getPickersByTeam('tl-unknown');

            expect(result).toEqual([]);
        });
    });

    // ═══════════════════════════════
    // addPicker
    // ═══════════════════════════════
    describe('addPicker', () => {
        it('inserts a new picker', async () => {
            const newPicker = {
                id: 'new-uuid',
                name: 'Charlie',
                role: 'picker',
                orchard_id: 'orchard-1',
                team_leader_id: 'tl-1',
                picker_id: 'PK003',
            };
            const chain = createChainMock(newPicker);
            mockFrom.mockReturnValue(chain);

            const result = await pickerService.addPicker({
                name: 'Charlie',
                orchard_id: 'orchard-1',
                team_leader_id: 'tl-1',
                picker_id: 'PK003',
            });

            expect(mockFrom).toHaveBeenCalledWith('pickers');
            expect(result).toEqual(newPicker);
        });

        it('throws on insert error', async () => {
            const chain = createChainMock(null, { message: 'Duplicate picker_id' });
            mockFrom.mockReturnValue(chain);

            await expect(
                pickerService.addPicker({ name: 'Duplicate' })
            ).rejects.toEqual({ message: 'Duplicate picker_id' });
        });
    });

    // ═══════════════════════════════
    // assignRowToPickers
    // ═══════════════════════════════
    describe('assignRowToPickers', () => {
        it('updates current_row for multiple pickers', async () => {
            const chain = createChainMock([{ id: 'p-1' }, { id: 'p-2' }]);
            mockFrom.mockReturnValue(chain);

            await pickerService.assignRowToPickers(['p-1', 'p-2'], 7);

            expect(mockFrom).toHaveBeenCalledWith('pickers');
            expect(chain.update).toHaveBeenCalledWith({ current_row: 7 });
        });
    });

    // ═══════════════════════════════
    // updatePickerStatus — uses .match()
    // ═══════════════════════════════
    describe('updatePickerStatus', () => {
        it('updates picker status via match', async () => {
            const chain = createChainMock(null, null);
            mockFrom.mockReturnValue(chain);

            await pickerService.updatePickerStatus('p-1', 'break');

            expect(chain.update).toHaveBeenCalledWith({ status: 'break' });
            expect(chain.match).toHaveBeenCalledWith({ id: 'p-1' });
        });

        it('throws on error', async () => {
            const chain = createChainMock(null, { message: 'Not found' });
            mockFrom.mockReturnValue(chain);

            await expect(
                pickerService.updatePickerStatus('nonexistent', 'active')
            ).rejects.toEqual({ message: 'Not found' });
        });
    });

    // ═══════════════════════════════
    // deletePicker — uses .delete().match()
    // ═══════════════════════════════
    describe('deletePicker', () => {
        it('hard deletes picker via match', async () => {
            const chain = createChainMock(null, null);
            mockFrom.mockReturnValue(chain);

            await pickerService.deletePicker('p-1');

            expect(chain.delete).toHaveBeenCalled();
            expect(chain.match).toHaveBeenCalledWith({ id: 'p-1' });
        });

        it('throws on delete error', async () => {
            const chain = createChainMock(null, { message: 'RLS denied' });
            mockFrom.mockReturnValue(chain);

            await expect(
                pickerService.deletePicker('p-1')
            ).rejects.toEqual({ message: 'RLS denied' });
        });
    });

    // ═══════════════════════════════
    // updatePickerRow
    // ═══════════════════════════════
    describe('updatePickerRow', () => {
        it('updates picker current_row', async () => {
            const chain = createChainMock(null, null);
            mockFrom.mockReturnValue(chain);

            await pickerService.updatePickerRow('p-1', 12);

            expect(chain.update).toHaveBeenCalledWith({ current_row: 12 });
        });
    });

    // ═══════════════════════════════
    // updatePicker — uses .match(), with duplicate check
    // ═══════════════════════════════
    describe('updatePicker', () => {
        it('updates picker with partial data (no picker_id change)', async () => {
            // Only 1 Supabase call: the update
            const chain = createChainMock(null, null);
            mockFrom.mockReturnValue(chain);

            await pickerService.updatePicker('p-1', { name: 'Alice Updated' });

            expect(mockFrom).toHaveBeenCalledWith('pickers');
            expect(chain.match).toHaveBeenCalledWith({ id: 'p-1' });
        });

        it('checks for duplicate picker_id before update', async () => {
            // First call: duplicate check (no duplicate found)
            const dupCheckChain = createChainMock(null, null);
            // Second call: actual update
            const updateChain = createChainMock(null, null);

            mockFrom
                .mockReturnValueOnce(dupCheckChain)
                .mockReturnValueOnce(updateChain);

            await pickerService.updatePicker('p-1', { picker_id: 'PK-NEW' });

            expect(mockFrom).toHaveBeenCalledTimes(2);
        });

        it('throws if duplicate picker_id found', async () => {
            // Duplicate check returns a result
            const dupCheckChain = createChainMock({ id: 'p-other', name: 'Bob' });
            mockFrom.mockReturnValue(dupCheckChain);

            await expect(
                pickerService.updatePicker('p-1', { picker_id: 'PK-DUPLICATE' })
            ).rejects.toThrow('already assigned to Bob');
        });

        it('throws on update error', async () => {
            const chain = createChainMock(null, { message: 'RLS denied' });
            mockFrom.mockReturnValue(chain);

            await expect(
                pickerService.updatePicker('p-1', { name: 'Denied' })
            ).rejects.toEqual({ message: 'RLS denied' });
        });
    });

    // ═══════════════════════════════
    // addPickersBulk
    // ═══════════════════════════════
    describe('addPickersBulk', () => {
        it('inserts pickers in bulk', async () => {
            const insertedData = [
                { id: 'uuid-1' },
                { id: 'uuid-2' },
            ];
            const chain = createChainMock(insertedData);
            mockFrom.mockReturnValue(chain);

            const pickers = [
                { name: 'Picker1', email: 'p1@test.com' },
                { name: 'Picker2', email: 'p2@test.com' },
            ];

            const result = await pickerService.addPickersBulk(pickers, 'orchard-1');

            expect(result.created).toBe(2);
            expect(result.errors).toHaveLength(0);
        });

        it('handles empty array input', async () => {
            const result = await pickerService.addPickersBulk([], 'orchard-1');

            expect(result.created).toBe(0);
            expect(result.errors).toHaveLength(0);
        });
    });
});
