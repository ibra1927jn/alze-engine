/**
 * useOrchardMap Hook Tests — pure logic
 */
import { describe, it, expect } from 'vitest';

// Test the pure computation logic from useOrchardMap
interface MockBlock {
    id: string;
    startRow: number;
    totalRows: number;
    rowVarieties: Record<number, string>;
}

const getBlockVarieties = (block: MockBlock): string[] =>
    Array.from(new Set(Object.values(block.rowVarieties)));

const calcBlockStats = (
    block: MockBlock,
    bucketsByRow: Record<number, number>,
    pickersByRow: Record<number, number>,
    targetBucketsPerRow: number,
) => {
    let buckets = 0, pickers = 0, completedRows = 0;
    for (let row = block.startRow; row < block.startRow + block.totalRows; row++) {
        const rowBuckets = bucketsByRow[row] || 0;
        buckets += rowBuckets;
        pickers += pickersByRow[row] || 0;
        if (rowBuckets >= targetBucketsPerRow) completedRows++;
    }
    return {
        buckets, activePickers: pickers, completedRows,
        progress: block.totalRows > 0 ? completedRows / block.totalRows : 0,
    };
};

describe('useOrchardMap — block varieties', () => {
    it('returns unique varieties', () => {
        const block: MockBlock = {
            id: 'b1', startRow: 1, totalRows: 4,
            rowVarieties: { 1: 'Cherry', 2: 'Cherry', 3: 'Apple', 4: 'Cherry' },
        };
        const varieties = getBlockVarieties(block);
        expect(varieties).toContain('Cherry');
        expect(varieties).toContain('Apple');
        expect(varieties).toHaveLength(2);
    });

    it('handles single variety', () => {
        const block: MockBlock = {
            id: 'b2', startRow: 1, totalRows: 2,
            rowVarieties: { 1: 'Cherry', 2: 'Cherry' },
        };
        expect(getBlockVarieties(block)).toEqual(['Cherry']);
    });
});

describe('useOrchardMap — block stats', () => {
    it('calculates block stats correctly', () => {
        const block: MockBlock = { id: 'b1', startRow: 1, totalRows: 3, rowVarieties: {} };
        const bucketsByRow = { 1: 50, 2: 30, 3: 50 };
        const pickersByRow = { 1: 3, 2: 2, 3: 4 };
        const stats = calcBlockStats(block, bucketsByRow, pickersByRow, 40);
        expect(stats.buckets).toBe(130);
        expect(stats.activePickers).toBe(9);
        expect(stats.completedRows).toBe(2); // rows 1 and 3 >= 40
        expect(stats.progress).toBeCloseTo(0.667, 2);
    });

    it('returns zero progress for empty block', () => {
        const block: MockBlock = { id: 'b2', startRow: 1, totalRows: 0, rowVarieties: {} };
        const stats = calcBlockStats(block, {}, {}, 40);
        expect(stats.progress).toBe(0);
    });

    it('handles missing rows gracefully', () => {
        const block: MockBlock = { id: 'b3', startRow: 5, totalRows: 2, rowVarieties: {} };
        const stats = calcBlockStats(block, {}, {}, 40);
        expect(stats.buckets).toBe(0);
        expect(stats.activePickers).toBe(0);
        expect(stats.completedRows).toBe(0);
    });
});
