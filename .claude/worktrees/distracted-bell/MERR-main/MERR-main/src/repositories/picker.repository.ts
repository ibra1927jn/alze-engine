/**
 * Picker Repository — Domain queries for pickers table
 */
import { supabase } from '@/services/supabase';

export const pickerRepository = {
    /** Update picker status */
    async updateStatus(pickerId: string, status: string) {
        const { error } = await supabase
            .from('pickers')
            .update({ status })
            .eq('id', pickerId);
        if (error) throw error;
    },

    /** Get performance view for today */
    async getPerformanceToday(orchardId?: string) {
        let query = supabase.from('pickers_performance_today').select('*');
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data, error } = await query;
        if (error) throw error;
        return data || [];
    },

    /** Get picker names by IDs */
    async getNamesByIds(ids: string[]): Promise<Record<string, string>> {
        if (ids.length === 0) return {};
        const { data } = await supabase
            .from('pickers')
            .select('id, name')
            .in('id', ids);
        return Object.fromEntries((data || []).map(p => [p.id, p.name]));
    },
};
