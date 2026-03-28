/**
 * Bucket Events Repository — for HarvestSyncBridge
 */
import { supabase } from '@/services/supabase';

export const bucketEventsRepository = {
    /** Batch insert bucket events */
    async insertBatch(rows: Record<string, unknown>[]) {
        const { error } = await supabase.from('bucket_events').insert(rows);
        return { error };
    },

    /** Insert single bucket event */
    async insertSingle(row: Record<string, unknown>) {
        const { error } = await supabase.from('bucket_events').insert(row);
        return { error };
    },
};
