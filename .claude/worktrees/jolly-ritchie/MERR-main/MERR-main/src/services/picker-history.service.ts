/**
 * Picker History Service
 * 
 * Fetches historical data for a picker: attendance, bucket records, quality,
 * and computes risk badges (fatigue, chronic top-up, quality drop).
 */
import { supabase } from '@/services/supabase';
import { logger } from '@/utils/logger';

// --- Types ---
export interface PickerProfile {
    id: string;
    picker_id: string;
    name: string;
    team_leader_id: string | null;
    team_leader_name: string | null;
    status: string;
    created_at: string;
}

export interface DailyRecord {
    date: string;
    buckets: number;
    hours: number;
    earnings: number;
    variety: string | null;
    team_leader_name: string | null;
}

export interface QualitySummary {
    total: number;
    gradeA: number;
    gradeB: number;
    gradeC: number;
    reject: number;
    score: number; // 0-100, A=100, B=70, C=40, reject=0
}

export interface RiskBadge {
    type: 'fatigue' | 'chronic_topup' | 'quality_drop' | 'anomalous_scans';
    severity: 'warning' | 'critical';
    label: string;
    detail: string;
}

export interface PickerHistory {
    profile: PickerProfile;
    todayBuckets: number;
    todayHours: number;
    todayEarnings: number;
    dailyRecords: DailyRecord[];
    quality: QualitySummary;
    riskBadges: RiskBadge[];
    teamLeadersWorkedWith: string[];
    varietiesPicked: string[];
}

// --- Service ---
class PickerHistoryService {

    async getPickerHistory(pickerId: string, orchardId: string, days: number = 14): Promise<PickerHistory | null> {
        try {
            // 1. Get picker profile
            const { data: picker } = await supabase
                .from('pickers')
                .select('*')
                .eq('id', pickerId)
                .single();

            if (!picker) {
                logger.warn('[PickerHistory] Picker not found:', pickerId);
                return null;
            }

            // 2. Get team leader name
            let teamLeaderName: string | null = null;
            if (picker.team_leader_id) {
                const { data: leader } = await supabase
                    .from('users')
                    .select('full_name')
                    .eq('id', picker.team_leader_id)
                    .single();
                teamLeaderName = leader?.full_name || null;
            }

            // 3. Get attendance (last N days)
            const since = new Date();
            since.setDate(since.getDate() - days);
            const sinceStr = since.toISOString().split('T')[0];

            const { data: attendance } = await supabase
                .from('daily_attendance')
                .select('*')
                .eq('picker_id', pickerId)
                .gte('date', sinceStr)
                .order('date', { ascending: false });

            // 4. Get bucket records (last N days)
            const { data: buckets } = await supabase
                .from('bucket_records')
                .select('*')
                .eq('picker_id', pickerId)
                .gte('scanned_at', since.toISOString())
                .order('scanned_at', { ascending: false });

            // 5. Get quality inspections
            const { data: inspections } = await supabase
                .from('quality_inspections')
                .select('*')
                .eq('picker_id', pickerId)
                .gte('created_at', since.toISOString());

            // 6. Get day setups for variety info
            const { data: daySetups } = await supabase
                .from('day_setups')
                .select('date, variety, piece_rate, min_wage_rate')
                .eq('orchard_id', orchardId)
                .gte('date', sinceStr)
                .order('date', { ascending: false });

            // --- Compute daily records ---
            const dailyMap = new Map<string, DailyRecord>();
            const today = new Date().toISOString().split('T')[0];

            // Initialize from attendance
            (attendance || []).forEach(a => {
                const hours = a.check_in_time && a.check_out_time
                    ? (new Date(a.check_out_time).getTime() - new Date(a.check_in_time).getTime()) / 3600000
                    : 0;
                const setup = (daySetups || []).find(d => d.date === a.date);
                dailyMap.set(a.date, {
                    date: a.date,
                    buckets: 0,
                    hours: Math.round(hours * 10) / 10,
                    earnings: 0,
                    variety: setup?.variety || null,
                    team_leader_name: teamLeaderName,
                });
            });

            // Count buckets per day
            (buckets || []).forEach(b => {
                const date = b.scanned_at.split('T')[0];
                const record = dailyMap.get(date) || {
                    date, buckets: 0, hours: 0, earnings: 0,
                    variety: null, team_leader_name: teamLeaderName
                };
                record.buckets++;
                dailyMap.set(date, record);
            });

            // Calculate earnings per day
            dailyMap.forEach((record, date) => {
                const setup = (daySetups || []).find(d => d.date === date);
                const pieceRate = setup?.piece_rate || 6.50;
                const minWage = setup?.min_wage_rate || 23.50;
                const pieceEarnings = record.buckets * pieceRate;
                const minWageEarnings = record.hours * minWage;
                record.earnings = Math.max(pieceEarnings, minWageEarnings);
            });

            const dailyRecords = Array.from(dailyMap.values())
                .sort((a, b) => a.date.localeCompare(b.date));

            // --- Today's stats ---
            const todayRecord = dailyMap.get(today);
            const todayBuckets = todayRecord?.buckets || picker.total_buckets_today || 0;
            const todayHours = todayRecord?.hours || 0;
            const todayEarnings = todayRecord?.earnings || 0;

            // --- Quality summary ---
            const quality = this.computeQuality(inspections || []);

            // --- Risk badges ---
            const riskBadges = this.computeRiskBadges(
                attendance || [],
                dailyRecords,
                quality,
                daySetups || []
            );

            // --- Team leaders worked with ---
            const teamLeadersWorkedWith = [...new Set(
                dailyRecords.map(r => r.team_leader_name).filter(Boolean) as string[]
            )];

            // --- Varieties picked ---
            const varietiesPicked = [...new Set(
                dailyRecords.map(r => r.variety).filter(Boolean) as string[]
            )];

            return {
                profile: {
                    id: picker.id,
                    picker_id: picker.picker_id,
                    name: picker.name,
                    team_leader_id: picker.team_leader_id,
                    team_leader_name: teamLeaderName,
                    status: picker.status,
                    created_at: picker.created_at,
                },
                todayBuckets,
                todayHours,
                todayEarnings,
                dailyRecords,
                quality,
                riskBadges,
                teamLeadersWorkedWith,
                varietiesPicked,
            };
        } catch (error) {
            logger.error('[PickerHistory] Error fetching history:', error);
            return null;
        }
    }

    private computeQuality(inspections: Array<{ quality_grade: string | null }>): QualitySummary {
        const total = inspections.length;
        if (total === 0) return { total: 0, gradeA: 0, gradeB: 0, gradeC: 0, reject: 0, score: 0 };

        const gradeA = inspections.filter(i => i.quality_grade === 'A' || i.quality_grade === 'good').length;
        const gradeB = inspections.filter(i => i.quality_grade === 'B' || i.quality_grade === 'warning').length;
        const gradeC = inspections.filter(i => i.quality_grade === 'C').length;
        const reject = inspections.filter(i => i.quality_grade === 'reject' || i.quality_grade === 'bad').length;
        const score = Math.round(((gradeA * 100 + gradeB * 70 + gradeC * 40) / total));

        return { total, gradeA, gradeB, gradeC, reject, score };
    }

    private computeRiskBadges(
        attendance: Array<{ date: string; status: string }>,
        dailyRecords: DailyRecord[],
        quality: QualitySummary,
        daySetups: Array<{ date: string; piece_rate: number; min_wage_rate: number }>
    ): RiskBadge[] {
        const badges: RiskBadge[] = [];

        // 🔋 Fatigue: 10+ consecutive days worked
        const sortedDates = attendance
            .filter(a => a.status === 'present' || a.status === 'late')
            .map(a => a.date)
            .sort();
        let consecutiveDays = 1;
        let maxConsecutive = 1;
        for (let i = 1; i < sortedDates.length; i++) {
            const prev = new Date(sortedDates[i - 1]);
            const curr = new Date(sortedDates[i]);
            const diff = (curr.getTime() - prev.getTime()) / 86400000;
            if (diff === 1) {
                consecutiveDays++;
                maxConsecutive = Math.max(maxConsecutive, consecutiveDays);
            } else {
                consecutiveDays = 1;
            }
        }
        if (maxConsecutive >= 10) {
            badges.push({
                type: 'fatigue',
                severity: maxConsecutive >= 14 ? 'critical' : 'warning',
                label: 'Fatigue Risk',
                detail: `${maxConsecutive} consecutive days worked without rest`,
            });
        }

        // 💸 Chronic Top-Up: needed min wage subsidy >60% of days
        if (dailyRecords.length >= 5) {
            let topUpDays = 0;
            dailyRecords.forEach(r => {
                const setup = daySetups.find(d => d.date === r.date);
                const pieceRate = setup?.piece_rate || 6.50;
                const minWage = setup?.min_wage_rate || 23.50;
                const pieceEarnings = r.buckets * pieceRate;
                const minWageEarnings = r.hours * minWage;
                if (r.hours > 0 && pieceEarnings < minWageEarnings) topUpDays++;
            });
            const topUpRatio = topUpDays / dailyRecords.length;
            if (topUpRatio > 0.6) {
                badges.push({
                    type: 'chronic_topup',
                    severity: topUpRatio > 0.8 ? 'critical' : 'warning',
                    label: 'Chronic Top-Up',
                    detail: `Needed wage subsidy ${Math.round(topUpRatio * 100)}% of days (${topUpDays}/${dailyRecords.length})`,
                });
            }
        }

        // 📉 Quality Drop: score < 50
        if (quality.total >= 3 && quality.score < 50) {
            badges.push({
                type: 'quality_drop',
                severity: quality.score < 30 ? 'critical' : 'warning',
                label: 'Low Quality',
                detail: `Quality score ${quality.score}/100 (${quality.reject} rejects)`,
            });
        }

        return badges;
    }
}

export const pickerHistoryService = new PickerHistoryService();
