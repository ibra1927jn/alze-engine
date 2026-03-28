/**
 * Picker CRUD Repository — full CRUD operations for pickers table
 * Extends the simpler pickerRepository with write operations.
 */
import { supabase } from '@/services/supabase';

export const pickerCrudRepository = {
    /** Query pickers with OR logic */
    async query(teamLeaderId?: string, orchardId?: string) {
        let query = supabase.from('pickers').select('*');
        if (teamLeaderId && orchardId) {
            query = query.or(`team_leader_id.eq.${teamLeaderId},orchard_id.eq.${orchardId}`);
        } else if (teamLeaderId) {
            query = query.eq('team_leader_id', teamLeaderId);
        } else if (orchardId) {
            query = query.eq('orchard_id', orchardId);
        }
        const { data, error } = await query;
        if (error) throw error;
        return data || [];
    },

    /** Get total picker count (diagnostic) */
    async getTotalCount() {
        const { count } = await supabase.from('pickers').select('*', { count: 'exact', head: true });
        return count;
    },

    /** Bulk update rows */
    async bulkUpdateRow(pickerIds: string[], row: number) {
        if (!pickerIds.length) return;
        const { error } = await supabase.from('pickers').update({ current_row: row }).in('id', pickerIds);
        if (error) throw error;
    },

    /** Insert picker */
    async insert(record: Record<string, unknown>) {
        const { data, error } = await supabase.from('pickers').insert([record]).select().single();
        if (error) throw error;
        return data;
    },

    /** Update picker by id (match) */
    async updateById(pickerId: string, updates: Record<string, unknown>) {
        const { error } = await supabase.from('pickers').update(updates).match({ id: pickerId });
        if (error) throw error;
    },

    /** Delete picker */
    async deleteById(pickerId: string) {
        const { error } = await supabase.from('pickers').delete().match({ id: pickerId });
        if (error) throw error;
    },

    /** Check duplicate picker_id */
    async findDuplicate(pickerId: string, excludeId: string) {
        const { data } = await supabase.from('pickers')
            .select('id, name').eq('picker_id', pickerId).eq('status', 'active').neq('id', excludeId).single();
        return data;
    },

    /** Bulk insert pickers */
    async insertBatch(rows: Record<string, unknown>[]) {
        const { data, error } = await supabase.from('pickers').insert(rows).select('id');
        if (error) throw error;
        return data;
    },

    /** Insert single (for fallback) */
    async insertSingle(row: Record<string, unknown>) {
        const { error } = await supabase.from('pickers').insert(row).select('id');
        return { error };
    },
};
