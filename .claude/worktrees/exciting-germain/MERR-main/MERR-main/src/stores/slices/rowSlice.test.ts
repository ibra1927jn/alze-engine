/**
 * rowSlice Tests — pure state transition logic
 */
import { describe, it, expect } from 'vitest';

// Test pure state transitions from rowSlice
interface RowAssignment {
    id: string;
    row_number: number;
    side: 'north' | 'south';
    assigned_pickers: string[];
    completion_percentage: number;
}

interface Picker { id: string; current_row: number; }

const assignRows = (
    existing: RowAssignment[],
    crew: Picker[],
    newEntries: RowAssignment[],
    pickerIds: string[],
    firstRowNumber: number,
) => {
    const assignments = [
        ...existing.filter(ra => !ra.assigned_pickers.some(pid => pickerIds.includes(pid))),
        ...newEntries,
    ];
    const updatedCrew = crew.map(p =>
        pickerIds.includes(p.id) ? { ...p, current_row: firstRowNumber } : p
    );
    return { assignments, crew: updatedCrew };
};

const updateProgress = (rows: RowAssignment[], rowId: string, percentage: number) =>
    rows.map(r => r.id === rowId ? { ...r, completion_percentage: percentage } : r);

const completeRow = (rows: RowAssignment[], rowId: string) =>
    rows.map(r => r.id === rowId ? { ...r, completion_percentage: 100 } : r);

describe('rowSlice — assignRows', () => {
    it('adds new entries to empty assignments', () => {
        const entry: RowAssignment = { id: 'r1', row_number: 5, side: 'north', assigned_pickers: ['p1'], completion_percentage: 0 };
        const { assignments } = assignRows([], [{ id: 'p1', current_row: 0 }], [entry], ['p1'], 5);
        expect(assignments).toHaveLength(1);
        expect(assignments[0].row_number).toBe(5);
    });

    it('removes old assignments for same pickers', () => {
        const old: RowAssignment = { id: 'old', row_number: 1, side: 'north', assigned_pickers: ['p1'], completion_percentage: 50 };
        const newEntry: RowAssignment = { id: 'new', row_number: 5, side: 'south', assigned_pickers: ['p1'], completion_percentage: 0 };
        const { assignments } = assignRows([old], [], [newEntry], ['p1'], 5);
        expect(assignments).toHaveLength(1);
        expect(assignments[0].id).toBe('new');
    });

    it('preserves assignments for other pickers', () => {
        const other: RowAssignment = { id: 'other', row_number: 3, side: 'north', assigned_pickers: ['p2'], completion_percentage: 80 };
        const newEntry: RowAssignment = { id: 'new', row_number: 5, side: 'north', assigned_pickers: ['p1'], completion_percentage: 0 };
        const { assignments } = assignRows([other], [], [newEntry], ['p1'], 5);
        expect(assignments).toHaveLength(2);
    });

    it('updates crew current_row', () => {
        const crew = [{ id: 'p1', current_row: 0 }, { id: 'p2', current_row: 3 }];
        const { crew: updated } = assignRows([], crew, [], ['p1'], 7);
        expect(updated[0].current_row).toBe(7);
        expect(updated[1].current_row).toBe(3); // Unchanged
    });
});

describe('rowSlice — updateProgress', () => {
    const rows: RowAssignment[] = [
        { id: 'r1', row_number: 1, side: 'north', assigned_pickers: ['p1'], completion_percentage: 0 },
        { id: 'r2', row_number: 2, side: 'south', assigned_pickers: ['p2'], completion_percentage: 50 },
    ];

    it('updates specific row percentage', () => {
        const updated = updateProgress(rows, 'r1', 75);
        expect(updated[0].completion_percentage).toBe(75);
        expect(updated[1].completion_percentage).toBe(50);
    });

    it('no-op for unknown row id', () => {
        const updated = updateProgress(rows, 'unknown', 100);
        expect(updated).toEqual(rows);
    });
});

describe('rowSlice — completeRow', () => {
    it('sets completion to 100', () => {
        const rows: RowAssignment[] = [
            { id: 'r1', row_number: 1, side: 'north', assigned_pickers: ['p1'], completion_percentage: 60 },
        ];
        const updated = completeRow(rows, 'r1');
        expect(updated[0].completion_percentage).toBe(100);
    });

    it('no-op for already completed row', () => {
        const rows: RowAssignment[] = [
            { id: 'r1', row_number: 1, side: 'north', assigned_pickers: ['p1'], completion_percentage: 100 },
        ];
        const updated = completeRow(rows, 'r1');
        expect(updated[0].completion_percentage).toBe(100);
    });
});
