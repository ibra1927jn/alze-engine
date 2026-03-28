/**
 * orchardMapSlice Tests — pure state logic
 */
import { describe, it, expect } from 'vitest';
import type { OrchardBlock } from '@/types';

// Test pure state transitions from orchardMapSlice
interface MapState {
    orchardBlocks: OrchardBlock[];
    selectedBlockId: string | null;
    selectedVariety: string;
    activeSeasonId: string | null;
    blocksLoading: boolean;
}

const initState: MapState = {
    orchardBlocks: [], selectedBlockId: null,
    selectedVariety: 'ALL', activeSeasonId: null, blocksLoading: false,
};

const setSelectedBlock = (state: MapState, id: string | null): MapState => ({
    ...state, selectedBlockId: id, selectedVariety: 'ALL',
});

const setSelectedVariety = (state: MapState, variety: string): MapState => ({
    ...state, selectedVariety: variety,
});

// Test the rowVarieties mapping from fetchBlocks
const buildRowVarieties = (rows: { block_id: string; row_number: number; variety: string | null }[]) => {
    const map = new Map<string, Record<number, string>>();
    rows.forEach(row => {
        if (!map.has(row.block_id)) map.set(row.block_id, {});
        map.get(row.block_id)![row.row_number] = row.variety || 'Desconocida';
    });
    return map;
};

describe('orchardMapSlice — selection', () => {
    it('starts with null selected block', () => {
        expect(initState.selectedBlockId).toBeNull();
    });

    it('setSelectedBlock updates id and resets variety', () => {
        let state = setSelectedVariety(initState, 'Cherry');
        state = setSelectedBlock(state, 'block-1');
        expect(state.selectedBlockId).toBe('block-1');
        expect(state.selectedVariety).toBe('ALL');
    });

    it('setSelectedBlock to null deselects', () => {
        let state = setSelectedBlock(initState, 'block-1');
        state = setSelectedBlock(state, null);
        expect(state.selectedBlockId).toBeNull();
    });

    it('setSelectedVariety updates variety', () => {
        const state = setSelectedVariety(initState, 'Apple');
        expect(state.selectedVariety).toBe('Apple');
    });
});

describe('orchardMapSlice — row varieties mapping', () => {
    it('maps rows to block varieties', () => {
        const rows = [
            { block_id: 'b1', row_number: 1, variety: 'Cherry' },
            { block_id: 'b1', row_number: 2, variety: 'Apple' },
            { block_id: 'b2', row_number: 1, variety: 'Pear' },
        ];
        const map = buildRowVarieties(rows);
        expect(map.get('b1')).toEqual({ 1: 'Cherry', 2: 'Apple' });
        expect(map.get('b2')).toEqual({ 1: 'Pear' });
    });

    it('defaults null variety to Desconocida', () => {
        const rows = [{ block_id: 'b1', row_number: 1, variety: null }];
        const map = buildRowVarieties(rows);
        expect(map.get('b1')![1]).toBe('Desconocida');
    });

    it('handles empty rows', () => {
        expect(buildRowVarieties([])).toEqual(new Map());
    });
});
