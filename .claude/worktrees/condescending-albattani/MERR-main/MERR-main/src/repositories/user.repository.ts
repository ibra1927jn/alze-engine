/**
 * User Repository — Domain queries for users table
 */
import { supabase } from '@/services/supabase';

export const userRepository2 = {
    /** Get all users (optionally filtered by orchard) */
    async getAll(orchardId?: string) {
        let query = supabase.from('users').select('*').order('full_name');
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data, error } = await query;
        if (error) throw error;
        return data || [];
    },

    /** Get active user count */
    async getActiveCount(orchardId?: string) {
        let query = supabase.from('users').select('id, is_active');
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data?.filter(u => u.is_active).length || 0;
    },

    /** Get user names by IDs */
    async getNamesByIds(ids: string[]): Promise<Record<string, string>> {
        if (ids.length === 0) return {};
        const { data } = await supabase
            .from('users')
            .select('id, full_name')
            .in('id', ids);
        return Object.fromEntries((data || []).map(u => [u.id, u.full_name || 'Unknown']));
    },
};
