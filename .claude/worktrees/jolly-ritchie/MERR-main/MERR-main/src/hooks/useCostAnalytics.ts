/**
 * useCostAnalytics — Data loading and cost calculations for CostAnalyticsView
 */
import { useState, useEffect, useMemo } from 'react';
import { useHarvestStore } from '@/stores/useHarvestStore';
import { payrollService, PickerBreakdown } from '@/services/payroll.service';
import { analyticsService } from '@/services/analytics.service';
import { TrendDataPoint, DayMeta } from '@/components/charts/TrendLineChart';
import { logger } from '@/utils/logger';

export interface TeamCost {
    teamLeader: string;
    pickers: number;
    totalBuckets: number;
    totalHours: number;
    totalEarnings: number;
    costPerBin: number;
}

export function useCostAnalytics() {
    const orchardId = useHarvestStore(s => s.orchard?.id);
    const settings = useHarvestStore(s => s.settings);
    const crew = useHarvestStore(s => s.crew);
    const openPickerProfile = useHarvestStore(s => s.openPickerProfile);

    const [pickers, setPickers] = useState<PickerBreakdown[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [costTrend, setCostTrend] = useState<TrendDataPoint[]>([]);
    const [selectedDayMeta, setSelectedDayMeta] = useState<DayMeta | null>(null);

    const breakEven = settings?.piece_rate;

    // Data loading
    useEffect(() => {
        const load = async () => {
            if (!orchardId) { setIsLoading(false); return; }
            setIsLoading(true);
            const payrollPromise = payrollService.calculateToday(orchardId)
                .then(result => setPickers(result.picker_breakdown))
                .catch(e => logger.warn('[CostAnalytics] Payroll failed:', e));
            const trendsPromise = analyticsService.getDailyTrends(orchardId, 7)
                .then(trends => { setCostTrend(trends.costPerBin); })
                .catch(e => logger.warn('[CostAnalytics] Trends failed:', e));
            await Promise.allSettled([payrollPromise, trendsPromise]);
            setIsLoading(false);
        };
        load();
    }, [orchardId]);

    // Aggregates
    const totalBuckets = pickers.reduce((sum, p) => sum + p.buckets, 0);
    const totalEarnings = pickers.reduce((sum, p) => sum + p.total_earnings, 0);
    const totalPieceRate = pickers.reduce((sum, p) => sum + (p.buckets * (settings?.piece_rate || 3.50)), 0);
    const totalTopUp = Math.max(0, totalEarnings - totalPieceRate);
    const costPerBin = totalBuckets > 0 ? totalEarnings / totalBuckets : 0;

    // Team costs
    const teamCosts = useMemo<TeamCost[]>(() => {
        const teamMap = new Map<string, PickerBreakdown[]>();
        pickers.forEach(p => {
            const crewMember = crew.find(c => c.picker_id === p.picker_id || c.name === p.picker_name);
            const leaderId = crewMember?.team_leader_id || 'unassigned';
            const leader = crew.find(c => c.id === leaderId);
            const teamName = leader?.name || 'Unassigned';
            if (!teamMap.has(teamName)) teamMap.set(teamName, []);
            teamMap.get(teamName)!.push(p);
        });
        return Array.from(teamMap.entries()).map(([teamLeader, members]) => {
            const tb = members.reduce((s, m) => s + m.buckets, 0);
            const th = members.reduce((s, m) => s + m.hours_worked, 0);
            const te = members.reduce((s, m) => s + m.total_earnings, 0);
            return { teamLeader, pickers: members.length, totalBuckets: tb, totalHours: th, totalEarnings: te, costPerBin: tb > 0 ? te / tb : 0 };
        }).sort((a, b) => a.costPerBin - b.costPerBin);
    }, [pickers, crew]);

    // Efficiency ranking
    const sortedByEfficiency = useMemo(() =>
        [...pickers].filter(p => p.buckets > 0).sort((a, b) => (a.total_earnings / a.buckets) - (b.total_earnings / b.buckets)),
        [pickers]);

    const maxCostPerBin = Math.max(...teamCosts.map(t => t.costPerBin), costPerBin || 1);

    const openProfile = (pickerId: string) => {
        const picker = crew.find(c => c.picker_id === pickerId);
        if (picker) openPickerProfile(picker.id);
    };

    return {
        isLoading, pickers, costTrend, selectedDayMeta, setSelectedDayMeta,
        breakEven, totalBuckets, totalEarnings, totalPieceRate, totalTopUp, costPerBin,
        teamCosts, sortedByEfficiency, maxCostPerBin, openProfile,
    };
}
