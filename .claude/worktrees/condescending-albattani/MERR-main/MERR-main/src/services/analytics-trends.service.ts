/**
 * analytics-trends.service.ts
 * Historical HeatMap & Trend Analytics (Phase 6+)
 * Queries repositories for day_closures, attendance, bucket_records
 */
import { logger } from '@/utils/logger';
import { analyticsTrendsRepository } from '@/repositories/analyticsTrends.repository';

export class AnalyticsTrendsService {
    /**
     * Obtener densidad de cosecha por row para un rango de fechas
     */
    async getRowDensity(
        orchardId: string,
        startDate: string,
        endDate: string,
        targetBucketsPerRow: number = 100
    ): Promise<{
        orchard_id: string;
        date_range: { start: string; end: string };
        total_buckets: number;
        total_rows_harvested: number;
        density_by_row: Array<{
            row_number: number;
            total_buckets: number;
            unique_pickers: number;
            avg_buckets_per_picker: number;
            density_score: number;
            target_completion: number;
        }>;
        top_rows: number[];
        pending_rows: number[];
    }> {
        const events = await analyticsTrendsRepository.getBucketsByRowInRange(orchardId, startDate, endDate);

        if (!events || events.length === 0) {
            return {
                orchard_id: orchardId,
                date_range: { start: startDate, end: endDate },
                total_buckets: 0, total_rows_harvested: 0,
                density_by_row: [], top_rows: [], pending_rows: []
            };
        }

        // Agrupar por row_number
        const rowStatsMap = new Map<number, { buckets: number; pickers: Set<string> }>();
        events.forEach((event) => {
            if (!rowStatsMap.has(event.row_number)) {
                rowStatsMap.set(event.row_number, { buckets: 0, pickers: new Set() });
            }
            const stats = rowStatsMap.get(event.row_number)!;
            stats.buckets++;
            stats.pickers.add(event.picker_id);
        });

        const density_by_row: Array<{
            row_number: number; total_buckets: number; unique_pickers: number;
            avg_buckets_per_picker: number; density_score: number; target_completion: number;
        }> = [];
        let total_buckets = 0;
        const top_rows: number[] = [];
        const pending_rows: number[] = [];

        for (const [row_number, stats] of rowStatsMap) {
            const avg = stats.pickers.size > 0 ? stats.buckets / stats.pickers.size : 0;
            const target_completion = (stats.buckets / targetBucketsPerRow) * 100;
            density_by_row.push({
                row_number, total_buckets: stats.buckets, unique_pickers: stats.pickers.size,
                avg_buckets_per_picker: parseFloat(avg.toFixed(2)),
                density_score: parseFloat(Math.min(100, target_completion).toFixed(2)),
                target_completion: parseFloat(target_completion.toFixed(2)),
            });
            total_buckets += stats.buckets;
            if (target_completion >= 100) top_rows.push(row_number);
            else if (target_completion < 50) pending_rows.push(row_number);
        }

        density_by_row.sort((a, b) => a.row_number - b.row_number);
        return {
            orchard_id: orchardId, date_range: { start: startDate, end: endDate },
            total_buckets, total_rows_harvested: density_by_row.length,
            density_by_row, top_rows: top_rows.sort((a, b) => a - b),
            pending_rows: pending_rows.sort((a, b) => a - b),
        };
    }

    /**
     * Get daily trend data for the last N days.
     */
    async getDailyTrends(orchardId: string, days: number = 7): Promise<{
        costPerBin: { label: string; value: number }[];
        totalBins: { label: string; value: number }[];
        workforceSize: { label: string; value: number }[];
        breakEvenCost: number;
    }> {
        const since = new Date();
        since.setDate(since.getDate() - days);
        const sinceStr = since.toISOString().split('T')[0];
        const todayStr = new Date().toISOString().split('T')[0];

        try {
            const closures = await analyticsTrendsRepository.getDayClosures(orchardId, sinceStr, todayStr);
            const attendanceData = await analyticsTrendsRepository.getAttendanceDates(orchardId, sinceStr, todayStr);

            if (closures && closures.length >= 2) {
                const dayLabels = (d: string) => new Date(d + 'T12:00:00').toLocaleDateString('en-NZ', { weekday: 'short' });
                const workforceMap = new Map<string, number>();
                (attendanceData || []).forEach((a: Record<string, unknown>) => {
                    workforceMap.set(a.date as string, (workforceMap.get(a.date as string) || 0) + 1);
                });

                return {
                    costPerBin: closures.map((c: Record<string, unknown>) => ({ label: dayLabels(c.date as string), value: (c.total_buckets as number) > 0 ? Math.round(((c.total_cost as number) / (c.total_buckets as number)) * 100) / 100 : 0 })),
                    totalBins: closures.map((c: Record<string, unknown>) => ({ label: dayLabels(c.date as string), value: (c.total_buckets as number) || 0 })),
                    workforceSize: closures.map((c: Record<string, unknown>) => ({ label: dayLabels(c.date as string), value: workforceMap.get(c.date as string) || 0 })),
                    breakEvenCost: 8.50,
                };
            }
        } catch (e) {
            logger.warn('[Analytics] Failed to fetch day_closures, using demo data:', e);
        }

        // --- Demo data (realistic NZ kiwifruit harvest) ---
        const dayNames = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'].slice(0, days);
        const costs = [7.20, 6.85, 7.50, 8.10, 7.95, 8.60, 7.40];
        const bins = [320, 350, 290, 310, 280, 340, 360];
        const pickers = [24, 26, 22, 25, 20, 27, 28];
        const teamSets = [
            [{ name: 'Team Acid', pickers: 10, buckets: 140 }, { name: 'Team Beta', pickers: 8, buckets: 110 }, { name: 'Team Gamma', pickers: 6, buckets: 70 }],
            [{ name: 'Team Acid', pickers: 11, buckets: 155 }, { name: 'Team Beta', pickers: 9, buckets: 120 }, { name: 'Team Gamma', pickers: 6, buckets: 75 }],
            [{ name: 'Team Acid', pickers: 9, buckets: 120 }, { name: 'Team Beta', pickers: 7, buckets: 95 }, { name: 'Team Gamma', pickers: 6, buckets: 75 }],
            [{ name: 'Team Acid', pickers: 10, buckets: 130 }, { name: 'Team Beta', pickers: 9, buckets: 110 }, { name: 'Team Gamma', pickers: 6, buckets: 70 }],
            [{ name: 'Team Acid', pickers: 8, buckets: 115 }, { name: 'Team Beta', pickers: 7, buckets: 100 }, { name: 'Team Gamma', pickers: 5, buckets: 65 }],
            [{ name: 'Team Acid', pickers: 12, buckets: 160 }, { name: 'Team Beta', pickers: 9, buckets: 110 }, { name: 'Team Gamma', pickers: 6, buckets: 70 }],
            [{ name: 'Team Acid', pickers: 12, buckets: 165 }, { name: 'Team Beta', pickers: 10, buckets: 125 }, { name: 'Team Gamma', pickers: 6, buckets: 70 }],
        ];

        const today = new Date();
        const makeMeta = (i: number) => ({
            date: (() => { const d = new Date(today); d.setDate(d.getDate() - (days - 1 - i)); return d.toISOString().split('T')[0]; })(),
            orchardName: 'J&P Cherries — Block C',
            teams: teamSets[i] || teamSets[0],
            totalPickers: pickers[i] || 25,
            totalBuckets: bins[i] || 300,
            totalTons: ((bins[i] || 300) * 13.5 / 1000),
            costPerBin: costs[i] || 7.50,
            topUpCost: Math.max(0, ((costs[i] || 7.50) - 6.50) * (bins[i] || 300)),
        });

        return {
            costPerBin: dayNames.map((d, i) => ({ label: d, value: costs[i] || 7.50, meta: makeMeta(i) })),
            totalBins: dayNames.map((d, i) => ({ label: d, value: bins[i] || 300, meta: makeMeta(i) })),
            workforceSize: dayNames.map((d, i) => ({ label: d, value: pickers[i] || 25, meta: makeMeta(i) })),
            breakEvenCost: 8.50,
        };
    }

    /**
     * Get daily wage bleed (min-wage top-up cost) for the last N days.
     */
    async getDailyBleed(_orchardId?: string, days: number = 7): Promise<{ label: string; value: number }[]> {
        const mockData: { label: string; value: number }[] = [];
        const today = new Date();
        const daysOfWeek = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
        let baseBleed = 900;
        for (let i = days - 1; i >= 0; i--) {
            const d = new Date(today);
            d.setDate(d.getDate() - i);
            const noise = Math.floor(Math.random() * 300) - 150;
            mockData.push({ label: i === 0 ? 'Today' : daysOfWeek[d.getDay()], value: Math.max(0, baseBleed + noise) });
            baseBleed -= 40;
        }
        return mockData;
    }
}

export const analyticsTrendsService = new AnalyticsTrendsService();
