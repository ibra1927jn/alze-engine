/**
 * QC Repository — Domain queries for qc_inspections table
 */
import { supabase } from '@/services/supabase';

export const qcRepository = {
    /** Insert a new inspection */
    async insert(payload: Record<string, unknown>) {
        const { data, error } = await supabase
            .from('qc_inspections')
            .insert(payload)
            .select()
            .single();
        return { data, error };
    },

    /** Get inspections for an orchard within a date range */
    async getByOrchardAndDateRange(orchardId: string, startISO: string, endISO: string) {
        const { data, error } = await supabase
            .from('qc_inspections')
            .select('*')
            .eq('orchard_id', orchardId)
            .gte('created_at', startISO)
            .lte('created_at', endISO)
            .order('created_at', { ascending: false });
        if (error) throw error;
        return data || [];
    },

    /** Get inspections for a specific picker */
    async getByPicker(pickerId: string, limit = 20) {
        const { data, error } = await supabase
            .from('qc_inspections')
            .select('*')
            .eq('picker_id', pickerId)
            .order('created_at', { ascending: false })
            .limit(limit);
        if (error) throw error;
        return data || [];
    },
};
