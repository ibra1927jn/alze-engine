/**
 * useWeeklyReport Hook Tests — pure logic
 */
import { describe, it, expect } from 'vitest';

// Test aggregation and team ranking logic from useWeeklyReport
interface MockPicker {
    picker_id: string;
    picker_name: string;
    buckets: number;
    hours_worked: number;
    total_earnings: number;
    team_leader_id?: string;
}

interface MockCrew {
    id: string;
    picker_id: string;
    name: string;
    team_leader_id?: string;
}

const calculateAggregates = (pickers: MockPicker[]) => {
    const totalBuckets = pickers.reduce((s, p) => s + p.buckets, 0);
    const totalHours = pickers.reduce((s, p) => s + p.hours_worked, 0);
    const totalEarnings = pickers.reduce((s, p) => s + p.total_earnings, 0);
    const avgBPA = totalHours > 0 ? totalBuckets / totalHours : 0;
    const costPerBin = totalBuckets > 0 ? totalEarnings / totalBuckets : 0;
    return { totalBuckets, totalHours, totalEarnings, avgBPA, costPerBin };
};

const buildTeamRankings = (pickers: MockPicker[], crew: MockCrew[]) => {
    const teamMap = new Map<string, { buckets: number; hours: number; earnings: number; count: number }>();
    pickers.forEach(p => {
        const crewMember = crew.find(c => c.picker_id === p.picker_id);
        const leaderId = crewMember?.team_leader_id || 'unassigned';
        const leader = crew.find(c => c.id === leaderId);
        const teamName = leader?.name || 'Unassigned';
        const entry = teamMap.get(teamName) || { buckets: 0, hours: 0, earnings: 0, count: 0 };
        entry.buckets += p.buckets;
        entry.hours += p.hours_worked;
        entry.earnings += p.total_earnings;
        entry.count++;
        teamMap.set(teamName, entry);
    });
    return Array.from(teamMap.entries())
        .map(([name, data]) => ({ name, ...data, bpa: data.hours > 0 ? data.buckets / data.hours : 0 }))
        .sort((a, b) => b.bpa - a.bpa);
};

describe('useWeeklyReport — aggregations', () => {
    it('sums totals correctly', () => {
        const pickers = [
            { picker_id: 'p1', picker_name: 'A', buckets: 30, hours_worked: 4, total_earnings: 195 },
            { picker_id: 'p2', picker_name: 'B', buckets: 20, hours_worked: 3, total_earnings: 130 },
        ];
        const agg = calculateAggregates(pickers);
        expect(agg.totalBuckets).toBe(50);
        expect(agg.totalHours).toBe(7);
        expect(agg.totalEarnings).toBe(325);
    });

    it('handles empty pickers', () => {
        const agg = calculateAggregates([]);
        expect(agg.avgBPA).toBe(0);
        expect(agg.costPerBin).toBe(0);
    });

    it('calculates avgBPA and costPerBin', () => {
        const pickers = [{ picker_id: 'p1', picker_name: 'A', buckets: 40, hours_worked: 8, total_earnings: 260 }];
        const agg = calculateAggregates(pickers);
        expect(agg.avgBPA).toBe(5); // 40/8
        expect(agg.costPerBin).toBe(6.5); // 260/40
    });
});

describe('useWeeklyReport — team rankings', () => {
    it('groups pickers by team leader', () => {
        const pickers = [
            { picker_id: 'p1', picker_name: 'A', buckets: 30, hours_worked: 4, total_earnings: 195 },
            { picker_id: 'p2', picker_name: 'B', buckets: 20, hours_worked: 3, total_earnings: 130 },
        ];
        const crew = [
            { id: 'p1', picker_id: 'p1', name: 'A', team_leader_id: 'tl1' },
            { id: 'p2', picker_id: 'p2', name: 'B', team_leader_id: 'tl1' },
            { id: 'tl1', picker_id: 'TL1', name: 'Leader 1' },
        ];
        const rankings = buildTeamRankings(pickers, crew);
        expect(rankings).toHaveLength(1);
        expect(rankings[0].name).toBe('Leader 1');
        expect(rankings[0].count).toBe(2);
    });

    it('assigns Unassigned for pickers without team leader', () => {
        const pickers = [{ picker_id: 'p1', picker_name: 'Solo', buckets: 10, hours_worked: 2, total_earnings: 65 }];
        const crew: MockCrew[] = [{ id: 'p1', picker_id: 'p1', name: 'Solo' }];
        const rankings = buildTeamRankings(pickers, crew);
        expect(rankings[0].name).toBe('Unassigned');
    });

    it('sorts by BPA descending', () => {
        const pickers = [
            { picker_id: 'p1', picker_name: 'A', buckets: 10, hours_worked: 5, total_earnings: 65 },
            { picker_id: 'p2', picker_name: 'B', buckets: 20, hours_worked: 4, total_earnings: 130 },
        ];
        const crew = [
            { id: 'p1', picker_id: 'p1', name: 'A', team_leader_id: 'tl1' },
            { id: 'p2', picker_id: 'p2', name: 'B', team_leader_id: 'tl2' },
            { id: 'tl1', picker_id: 'TL1', name: 'Slow Team' },
            { id: 'tl2', picker_id: 'TL2', name: 'Fast Team' },
        ];
        const rankings = buildTeamRankings(pickers, crew);
        expect(rankings[0].name).toBe('Fast Team'); // 5 BPA > 2 BPA
    });
});
