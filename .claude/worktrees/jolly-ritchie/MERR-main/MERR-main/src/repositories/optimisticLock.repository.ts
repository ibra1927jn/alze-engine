/**
 * Optimistic Lock Repository — Generic table update operations
 * Used for atomic conditional updates via WHERE clause.
 */
import { supabase } from '@/services/supabase';

export const optimisticLockRepository = {
    /** Perform a conditional update (optimistic lock pattern) */
    async conditionalUpdate(table: string, recordId: string, expectedUpdatedAt: string, updates: Record<string, unknown>) {
        const { data, error } = await supabase
            .from(table)
            .update(updates)
            .eq('id', recordId)
            .eq('updated_at', expectedUpdatedAt)
            .select()
            .single();
        return { data, error };
    },

    /** Get a record by ID */
    async getById(table: string, recordId: string) {
        const { data } = await supabase
            .from(table)
            .select('*')
            .eq('id', recordId)
            .single();
        return data;
    },

    /** Standard (non-locked) update */
    async update(table: string, recordId: string, updates: Record<string, unknown>) {
        const { data, error } = await supabase
            .from(table)
            .update(updates)
            .eq('id', recordId)
            .select()
            .single();
        if (error) throw error;
        return data;
    },
};
