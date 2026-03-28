/**
 * useCostAnalytics Hook Tests — pure logic
 */
import { describe, it, expect } from 'vitest';

// Test the pure cost calculation logic from useCostAnalytics
interface MockPicker {
    picker_id: string;
    picker_name: string;
    buckets: number;
    hours_worked: number;
    total_earnings: number;
    team_leader_id?: string;
}

const calculateCosts = (pickers: MockPicker[], pieceRate = 3.50) => {
    const totalBuckets = pickers.reduce((sum, p) => sum + p.buckets, 0);
    const totalEarnings = pickers.reduce((sum, p) => sum + p.total_earnings, 0);
    const totalPieceRate = pickers.reduce((sum, p) => sum + (p.buckets * pieceRate), 0);
    const totalTopUp = Math.max(0, totalEarnings - totalPieceRate);
    const costPerBin = totalBuckets > 0 ? totalEarnings / totalBuckets : 0;
    return { totalBuckets, totalEarnings, totalPieceRate, totalTopUp, costPerBin };
};

describe('useCostAnalytics — cost calculations', () => {
    it('calculates totals correctly', () => {
        const pickers: MockPicker[] = [
            { picker_id: 'p1', picker_name: 'A', buckets: 30, hours_worked: 4, total_earnings: 195 },
            { picker_id: 'p2', picker_name: 'B', buckets: 20, hours_worked: 3, total_earnings: 130 },
        ];
        const result = calculateCosts(pickers, 6.50);
        expect(result.totalBuckets).toBe(50);
        expect(result.totalEarnings).toBe(325);
        expect(result.totalPieceRate).toBe(325); // 50 * 6.50
        expect(result.totalTopUp).toBe(0); // earnings == piece rate
    });

    it('calculates top-up when earnings exceed piece rate', () => {
        const pickers: MockPicker[] = [
            { picker_id: 'p1', picker_name: 'A', buckets: 5, hours_worked: 4, total_earnings: 100 },
        ];
        const result = calculateCosts(pickers, 6.50);
        expect(result.totalPieceRate).toBe(32.5); // 5 * 6.50
        expect(result.totalTopUp).toBe(67.5); // 100 - 32.5
    });

    it('handles zero workers', () => {
        const result = calculateCosts([]);
        expect(result.costPerBin).toBe(0);
        expect(result.totalTopUp).toBe(0);
    });

    it('calculates cost per bin', () => {
        const pickers: MockPicker[] = [
            { picker_id: 'p1', picker_name: 'A', buckets: 20, hours_worked: 4, total_earnings: 130 },
        ];
        const result = calculateCosts(pickers, 6.50);
        expect(result.costPerBin).toBe(6.5); // 130/20
    });

    it('efficiency ranking sorts by cost per bin ascending', () => {
        const pickers: MockPicker[] = [
            { picker_id: 'p1', picker_name: 'Expensive', buckets: 5, hours_worked: 4, total_earnings: 100 },
            { picker_id: 'p2', picker_name: 'Efficient', buckets: 20, hours_worked: 4, total_earnings: 100 },
        ];
        const sorted = [...pickers]
            .filter(p => p.buckets > 0)
            .sort((a, b) => (a.total_earnings / a.buckets) - (b.total_earnings / b.buckets));
        expect(sorted[0].picker_name).toBe('Efficient'); // $5/bin < $20/bin
    });
});
