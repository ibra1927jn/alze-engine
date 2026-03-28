/**
 * Contract Repository — Domain queries for contracts table
 */
import { supabase } from '@/services/supabase';

export const contractRepository2 = {
    /** Get pending/draft contracts */
    async getPending(orchardId?: string) {
        let query = supabase
            .from('contracts')
            .select('id, status')
            .in('status', ['draft', 'expiring']);
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data || [];
    },

    /** Get all contracts with full details (ordered by status + end_date) */
    async getAll(orchardId?: string) {
        let query = supabase
            .from('contracts')
            .select('id, employee_id, type, status, start_date, end_date, hourly_rate, notes, created_at, updated_at')
            .order('status')
            .order('end_date', { ascending: true, nullsFirst: false });
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data, error } = await query;
        if (error) throw error;
        return data || [];
    },

    /** Get contracts expiring within N days */
    async getExpiringSoon(orchardId: string | undefined, today: string, cutoffDate: string) {
        let query = supabase
            .from('contracts')
            .select('id, employee_id, type, end_date, hourly_rate')
            .not('end_date', 'is', null)
            .lte('end_date', cutoffDate)
            .gte('end_date', today)
            .in('status', ['active', 'expiring']);
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data || [];
    },

    /** Get expired contracts still marked active */
    async getExpiredButActive(orchardId?: string) {
        const today = new Date().toISOString().split('T')[0];
        let query = supabase
            .from('contracts')
            .select('id, employee_id')
            .lt('end_date', today)
            .eq('status', 'active');
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data || [];
    },

    /** Get contracts by employee IDs (for enrichment) */
    async getByEmployeeIds(employeeIds: string[]) {
        if (employeeIds.length === 0) return [];
        const { data } = await supabase
            .from('contracts')
            .select('employee_id, type, status, start_date, end_date, hourly_rate')
            .in('employee_id', employeeIds)
            .in('status', ['active', 'expiring']);
        return data || [];
    },
};
