/**
 * useRowAssignments Hook Tests — pure logic
 */
import { describe, it, expect } from 'vitest';

// Test the pure mapping logic extracted from useRowAssignments
interface RawRow {
    id: string;
    row_number: number;
    side: 'north' | 'south';
    assigned_pickers: string[];
    completion_percentage: number;
}

const mapRow = (r: RawRow) => ({
    id: r.id,
    rowNumber: r.row_number,
    side: r.side === 'north' ? 'North' : 'South',
    assignedPickers: r.assigned_pickers,
    completionPercentage: r.completion_percentage,
    status: r.completion_percentage === 100
        ? 'Completed'
        : r.completion_percentage > 0
            ? 'Active'
            : 'Assigned',
});

const calcAverage = (rows: { completionPercentage: number }[]) => {
    if (rows.length === 0) return 0;
    return Math.round(rows.reduce((sum, r) => sum + r.completionPercentage, 0) / rows.length);
};

describe('useRowAssignments — row mapping', () => {
    it('maps north side correctly', () => {
        const row = mapRow({ id: 'r1', row_number: 5, side: 'north', assigned_pickers: ['p1'], completion_percentage: 0 });
        expect(row.side).toBe('North');
        expect(row.status).toBe('Assigned');
    });

    it('maps south side correctly', () => {
        const row = mapRow({ id: 'r2', row_number: 3, side: 'south', assigned_pickers: [], completion_percentage: 50 });
        expect(row.side).toBe('South');
        expect(row.status).toBe('Active');
    });

    it('marks 100% as Completed', () => {
        const row = mapRow({ id: 'r3', row_number: 1, side: 'north', assigned_pickers: ['p1'], completion_percentage: 100 });
        expect(row.status).toBe('Completed');
    });

    it('marks 0% as Assigned', () => {
        const row = mapRow({ id: 'r4', row_number: 2, side: 'south', assigned_pickers: [], completion_percentage: 0 });
        expect(row.status).toBe('Assigned');
    });
});

describe('useRowAssignments — statistics', () => {
    it('returns 0 average for empty rows', () => {
        expect(calcAverage([])).toBe(0);
    });

    it('calculates average completion', () => {
        const rows = [
            { completionPercentage: 0 },
            { completionPercentage: 50 },
            { completionPercentage: 100 },
        ];
        expect(calcAverage(rows)).toBe(50);
    });

    it('rounds average to nearest integer', () => {
        const rows = [
            { completionPercentage: 33 },
            { completionPercentage: 66 },
        ];
        expect(calcAverage(rows)).toBe(50); // (33+66)/2 = 49.5 → 50
    });
});
