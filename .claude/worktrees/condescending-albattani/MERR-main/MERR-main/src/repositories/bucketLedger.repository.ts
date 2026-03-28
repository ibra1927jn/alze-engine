/**
 * Bucket Ledger Repository — Domain queries for bucket_records and picker resolution
 */
import { supabase } from '@/services/supabase';

export const bucketLedgerRepository = {
    /** Resolve a picker badge ID to UUID */
    async resolvePickerByBadge(pickerId: string, orchardId: string) {
        const { data } = await supabase
            .from('pickers')
            .select('id, picker_id')
            .eq('picker_id', pickerId)
            .eq('orchard_id', orchardId)
            .maybeSingle();
        return data;
    },

    /** Insert a bucket record */
    async insertBucketRecord(payload: Record<string, unknown>) {
        const { data, error } = await supabase
            .from('bucket_records')
            .insert([payload])
            .select()
            .single();
        return { data, error };
    },

    /** Get picker history */
    async getPickerHistory(pickerId: string, limit = 20) {
        const { data, error } = await supabase
            .from('bucket_records')
            .select('*')
            .eq('picker_id', pickerId)
            .order('scanned_at', { ascending: false })
            .limit(limit);
        if (error) throw error;
        return data;
    },
};
