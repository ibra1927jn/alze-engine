/**
 * Store Sync Repository — queries used by storeSync.ts for app boot hydration
 */
import { supabase } from '@/services/supabase';

export const storeSyncRepository = {
    /** Get first orchard */
    async getFirstOrchard() {
        const { data } = await supabase.from('orchards').select('*').limit(1);
        return data?.[0] || null;
    },

    /** Get harvest settings for an orchard */
    async getSettings(orchardId: string) {
        const { data } = await supabase
            .from('harvest_settings').select('*').eq('orchard_id', orchardId).single();
        return data;
    },

    /** Get pickers with attendance join — returns query builder for delta chaining */
    getPickersQuery(orchardId: string, today: string) {
        return supabase
            .from('pickers')
            .select(`*, daily_attendance!left(check_in_time, check_out_time)`)
            .eq('orchard_id', orchardId)
            .eq('daily_attendance.date', today);
    },

    /** Get bucket records since a date — returns query builder for delta chaining */
    getBucketRecordsQuery(orchardId: string, since: string) {
        return supabase
            .from('bucket_records').select('*').eq('orchard_id', orchardId)
            .gte('scanned_at', since).order('scanned_at', { ascending: false });
    },

    /** Get bucket records with count for payroll (hhrr.service remaining call) */
    async getBucketCounts(orchardId: string | undefined, since: string) {
        let query = supabase.from('bucket_records').select('picker_id, id').gte('scanned_at', since);
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data || [];
    },

    /** Get harvest settings piece_rate */
    async getPieceRate(orchardId: string) {
        const { data } = await supabase
            .from('harvest_settings').select('piece_rate').eq('orchard_id', orchardId).single();
        return data?.piece_rate || null;
    },
};
