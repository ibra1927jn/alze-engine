/**
 * intelligenceSlice - Stats, Payroll & Compliance
 * 
 * Recalculates payroll estimates, compliance alerts, and harvest stats.
 * Reads crew, buckets, and settings from global state via get().
 */
import { StateCreator } from 'zustand';
import { complianceService, ComplianceViolation } from '@/services/compliance.service';
import type { HarvestStoreState, IntelligenceSlice, HarvestStats } from '../storeTypes';
import { nowNZST } from '@/utils/nzst';

// --- Default State ---
const defaultStats: HarvestStats = {
    totalBuckets: 0,
    payEstimate: 0,
    tons: 0,
    velocity: 0,
    goalVelocity: 0,
    binsFull: 0,
};

// --- Slice Creator ---
export const createIntelligenceSlice: StateCreator<
    HarvestStoreState,
    [],
    [],
    IntelligenceSlice
> = (set, get) => ({
    alerts: [],
    payroll: { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
    notifications: [],
    stats: defaultStats,

    recalculateIntelligence: () => {
        const state = get();
        const { crew, settings } = state;

        // 1. Calculate Payroll
        const bucketCounts = new Map<string, number>();
        state.buckets.forEach(b => {
            bucketCounts.set(b.picker_id, (bucketCounts.get(b.picker_id) || 0) + 1);
        });

        // Filter out archived pickers
        const activeCrew = crew.filter(p => p.status !== 'archived');

        // Calculate payroll using local logic (same as Edge Function)
        let totalPiece = 0;
        let totalMinimum = 0;

        activeCrew.forEach(p => {
            const buckets = (bucketCounts.get(p.id) || 0) + (p.total_buckets_today || 0);
            const hours = p.hours || 0; // 🔧 L32: Never fabricate hours — 0 is honest
            const pieceEarnings = buckets * settings.piece_rate;
            const minimumWageThreshold = hours * settings.min_wage_rate;
            const minimumWageOwed = Math.max(0, minimumWageThreshold - pieceEarnings);
            totalPiece += pieceEarnings;
            totalMinimum += minimumWageOwed;
        });

        const payroll = {
            totalPiece,
            totalMinimum,
            finalTotal: totalPiece + totalMinimum,
        };

        // 2. Compliance Checks
        const alerts: ComplianceViolation[] = [];
        activeCrew.forEach(p => {
            const buckets = (bucketCounts.get(p.id) || 0) + (p.total_buckets_today || 0);
            const hours = p.hours || 0; // 🔧 L32: Never fabricate hours

            const status = complianceService.checkPickerCompliance({
                pickerId: p.id,
                bucketCount: buckets,
                hoursWorked: hours,
                consecutiveMinutesWorked: 120, // Mock
                totalMinutesToday: hours * 60,
                lastRestBreakAt: null,
                lastMealBreakAt: null,
                lastHydrationAt: null,
                // 🔧 L33: Use NZST instead of UTC Date.now()
                workStartTime: new Date(new Date(nowNZST()).getTime() - (hours * 3600000)),
            });

            if (status.violations.length > 0) {
                alerts.push(
                    ...status.violations.map(v => ({
                        ...v,
                        details: { ...v.details, pickerId: p.id, pickerName: p.name },
                    }))
                );
            }
        });

        set({ payroll, alerts });
    },
});
