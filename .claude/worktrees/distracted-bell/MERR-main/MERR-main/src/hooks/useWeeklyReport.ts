/**
 * useWeeklyReport — Data loading and calculations for WeeklyReportView
 * Extracts all state management, data fetching, and derived calculations.
 */
import { useState, useEffect, useMemo } from 'react';
import { useHarvestStore } from '@/stores/useHarvestStore';
import { payrollService, PickerBreakdown } from '@/services/payroll.service';
import { analyticsService } from '@/services/analytics.service';
import { TrendDataPoint, DayMeta } from '@/components/charts/TrendLineChart';
import { logger } from '@/utils/logger';

export interface TeamRanking {
    name: string;
    buckets: number;
    hours: number;
    earnings: number;
    count: number;
    bpa: number;
}

export interface WeeklyReportData {
    // Raw data
    pickers: PickerBreakdown[];
    binsTrend: TrendDataPoint[];
    workforceTrend: TrendDataPoint[];
    isLoading: boolean;

    // Aggregations
    totalBuckets: number;
    totalHours: number;
    totalEarnings: number;
    avgBPA: number;
    costPerBin: number;
    dailyBinTarget: number | undefined;

    // Team data
    teamRankings: TeamRanking[];

    // Day detail
    selectedDayMeta: DayMeta | null;
    setSelectedDayMeta: React.Dispatch<React.SetStateAction<DayMeta | null>>;

    // Export state
    showExportModal: boolean;
    setShowExportModal: React.Dispatch<React.SetStateAction<boolean>>;

    // Context
    orchard: { id: string; name?: string; total_rows?: number } | null;
    crew: { id: string; picker_id: string; name: string; team_leader_id?: string }[];

    // Actions
    openProfile: (pickerId: string) => void;
}

export function useWeeklyReport(): WeeklyReportData {
    const orchardId = useHarvestStore(s => s.orchard?.id);
    const orchard = useHarvestStore(s => s.orchard);
    const settings = useHarvestStore(s => s.settings);
    const crew = useHarvestStore(s => s.crew);
    const openPickerProfile = useHarvestStore(s => s.openPickerProfile);

    const dailyBinTarget = settings?.target_tons
        ? Math.round((settings.target_tons * 72) / 30)
        : undefined;

    const [pickers, setPickers] = useState<PickerBreakdown[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [binsTrend, setBinsTrend] = useState<TrendDataPoint[]>([]);
    const [workforceTrend, setWorkforceTrend] = useState<TrendDataPoint[]>([]);
    const [selectedDayMeta, setSelectedDayMeta] = useState<DayMeta | null>(null);
    const [showExportModal, setShowExportModal] = useState(false);

    useEffect(() => {
        const load = async () => {
            if (!orchardId) { setIsLoading(false); return; }
            setIsLoading(true);
            const payrollPromise = payrollService.calculateToday(orchardId)
                .then(result => setPickers(result.picker_breakdown))
                .catch(e => logger.warn('[WeeklyReport] Payroll failed:', e));
            const trendsPromise = analyticsService.getDailyTrends(orchardId, 7)
                .then(trends => { setBinsTrend(trends.totalBins); setWorkforceTrend(trends.workforceSize); })
                .catch(e => logger.warn('[WeeklyReport] Trends failed:', e));
            await Promise.allSettled([payrollPromise, trendsPromise]);
            setIsLoading(false);
        };
        load();
    }, [orchardId]);

    // Aggregations
    const totalBuckets = pickers.reduce((s, p) => s + p.buckets, 0);
    const totalHours = pickers.reduce((s, p) => s + p.hours_worked, 0);
    const totalEarnings = pickers.reduce((s, p) => s + p.total_earnings, 0);
    const avgBPA = totalHours > 0 ? totalBuckets / totalHours : 0;
    const costPerBin = totalBuckets > 0 ? totalEarnings / totalBuckets : 0;

    // Team rankings
    const teamRankings = useMemo(() => {
        const teamMap = new Map<string, { buckets: number; hours: number; earnings: number; count: number }>();
        pickers.forEach(p => {
            const crewMember = crew.find(c => c.picker_id === p.picker_id || c.name === p.picker_name);
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
    }, [pickers, crew]);

    const openProfile = (pickerId: string) => {
        const picker = crew.find(c => c.picker_id === pickerId);
        if (picker) openPickerProfile(picker.id);
    };

    return {
        pickers, binsTrend, workforceTrend, isLoading,
        totalBuckets, totalHours, totalEarnings, avgBPA, costPerBin, dailyBinTarget,
        teamRankings,
        selectedDayMeta, setSelectedDayMeta,
        showExportModal, setShowExportModal,
        orchard, crew,
        openProfile,
    };
}
