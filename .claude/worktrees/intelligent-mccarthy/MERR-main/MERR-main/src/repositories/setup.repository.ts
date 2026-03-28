/**
 * Setup Repository — Domain queries for orchards and day_setups tables
 */
import { supabase } from '@/services/supabase';

export const setupRepository = {
    /** Insert an orchard */
    async insertOrchard(payload: Record<string, unknown>) {
        const { data, error } = await supabase
            .from('orchards')
            .insert(payload)
            .select()
            .single();
        return { data, error };
    },

    /** Insert a day setup */
    async insertDaySetup(payload: Record<string, unknown>) {
        const { error } = await supabase
            .from('day_setups')
            .insert(payload);
        return { error };
    },
};
