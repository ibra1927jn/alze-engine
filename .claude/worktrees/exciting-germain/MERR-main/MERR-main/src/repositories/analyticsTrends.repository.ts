/**
 * Analytics Trends Repository — Domain queries for day_closures, daily_attendance, bucket_records
 */
import { supabase } from '@/services/supabase';

export const analyticsTrendsRepository = {
    /** Get bucket records for a date range grouped by row */
    async getBucketsByRowInRange(orchardId: string, startDate: string, endDate: string) {
        const { data, error } = await supabase
            .from('bucket_records')
            .select('row_number, picker_id, scanned_at')
            .eq('orchard_id', orchardId)
            .gte('scanned_at', `${startDate}T00:00:00`)
            .lte('scanned_at', `${endDate}T23:59:59`);
        if (error) throw error;
        return data || [];
    },

    /** Get day closures for a date range */
    async getDayClosures(orchardId: string, startDate: string, endDate: string) {
        const { data, error } = await supabase
            .from('day_closures')
            .select('*')
            .eq('orchard_id', orchardId)
            .gte('date', startDate)
            .lte('date', endDate)
            .order('date');
        if (error) throw error;
        return data || [];
    },

    /** Get distinct attendance dates for a date range */
    async getAttendanceDates(orchardId: string, startDate: string, endDate: string) {
        const { data, error } = await supabase
            .from('daily_attendance')
            .select('date, picker_id')
            .eq('orchard_id', orchardId)
            .gte('date', startDate)
            .lte('date', endDate);
        if (error) throw error;
        return data || [];
    },
};
