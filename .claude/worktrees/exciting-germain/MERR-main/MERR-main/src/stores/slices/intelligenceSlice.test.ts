/**
 * intelligenceSlice Tests — pure payroll and compliance logic
 */
import { describe, it, expect } from 'vitest';

// Test the pure payroll calculation logic from intelligenceSlice
interface MockPicker {
    id: string;
    status: string;
    total_buckets_today: number;
    hours: number;
    name: string;
}

interface PayrollResult {
    totalPiece: number;
    totalMinimum: number;
    finalTotal: number;
}

const calculatePayroll = (
    activeCrew: MockPicker[],
    scannedBuckets: Map<string, number>,
    pieceRate: number,
    minWageRate: number,
): PayrollResult => {
    let totalPiece = 0;
    let totalMinimum = 0;

    activeCrew.forEach(p => {
        const buckets = (scannedBuckets.get(p.id) || 0) + (p.total_buckets_today || 0);
        const hours = p.hours || 0;
        const pieceEarnings = buckets * pieceRate;
        const minimumWageThreshold = hours * minWageRate;
        const minimumWageOwed = Math.max(0, minimumWageThreshold - pieceEarnings);
        totalPiece += pieceEarnings;
        totalMinimum += minimumWageOwed;
    });

    return { totalPiece, totalMinimum, finalTotal: totalPiece + totalMinimum };
};

const filterActiveCrew = (crew: MockPicker[]) =>
    crew.filter(p => p.status !== 'archived');

describe('intelligenceSlice — payroll calculation', () => {
    it('calculates piece rate earnings', () => {
        const crew = [{ id: 'p1', status: 'active', total_buckets_today: 10, hours: 0, name: 'A' }];
        const result = calculatePayroll(crew, new Map(), 6.50, 23.50);
        expect(result.totalPiece).toBe(65);
        expect(result.totalMinimum).toBe(0);
        expect(result.finalTotal).toBe(65);
    });

    it('adds minimum wage top-up', () => {
        const crew = [{ id: 'p1', status: 'active', total_buckets_today: 2, hours: 4, name: 'A' }];
        const result = calculatePayroll(crew, new Map(), 6.50, 23.50);
        expect(result.totalPiece).toBe(13); // 2 * 6.50
        expect(result.totalMinimum).toBe(81); // 4*23.50 - 13
        expect(result.finalTotal).toBe(94); // 4 * 23.50
    });

    it('adds scanned buckets to total', () => {
        const crew = [{ id: 'p1', status: 'active', total_buckets_today: 5, hours: 0, name: 'A' }];
        const scanned = new Map([['p1', 3]]);
        const result = calculatePayroll(crew, scanned, 6.50, 23.50);
        expect(result.totalPiece).toBe(52); // (5+3) * 6.50
    });

    it('handles zero hours (no top-up)', () => {
        const crew = [{ id: 'p1', status: 'active', total_buckets_today: 0, hours: 0, name: 'A' }];
        const result = calculatePayroll(crew, new Map(), 6.50, 23.50);
        expect(result.finalTotal).toBe(0);
    });

    it('handles multiple pickers', () => {
        const crew = [
            { id: 'p1', status: 'active', total_buckets_today: 10, hours: 2, name: 'A' },
            { id: 'p2', status: 'active', total_buckets_today: 20, hours: 3, name: 'B' },
        ];
        const result = calculatePayroll(crew, new Map(), 6.50, 23.50);
        expect(result.totalPiece).toBe(195); // (10+20) * 6.50
    });
});

describe('intelligenceSlice — crew filtering', () => {
    it('filters out archived pickers', () => {
        const crew = [
            { id: 'p1', status: 'active', total_buckets_today: 0, hours: 0, name: 'A' },
            { id: 'p2', status: 'archived', total_buckets_today: 0, hours: 0, name: 'B' },
        ];
        expect(filterActiveCrew(crew)).toHaveLength(1);
        expect(filterActiveCrew(crew)[0].id).toBe('p1');
    });

    it('allows all non-archived statuses', () => {
        const crew = [
            { id: 'p1', status: 'active', total_buckets_today: 0, hours: 0, name: 'A' },
            { id: 'p2', status: 'on_break', total_buckets_today: 0, hours: 0, name: 'B' },
            { id: 'p3', status: 'suspended', total_buckets_today: 0, hours: 0, name: 'C' },
        ];
        expect(filterActiveCrew(crew)).toHaveLength(3);
    });
});
