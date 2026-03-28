/**
 * Logistics Repository — Domain queries for bins, fleet_vehicles, transport_requests
 */
import { supabase } from '@/services/supabase';

export const logisticsRepository = {
    /* ── Bins ── */
    async getBinStatuses(orchardId?: string) {
        let query = supabase.from('bins').select('status');
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data || [];
    },

    async getBinInventory(orchardId?: string, limit = 50) {
        let query = supabase.from('bins').select('*')
            .order('created_at', { ascending: false }).limit(limit);
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data, error } = await query;
        if (error) throw error;
        return data || [];
    },

    /* ── Fleet ── */
    async getFleetStatuses(orchardId?: string) {
        let query = supabase.from('fleet_vehicles').select('status');
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data || [];
    },

    async getFleetAll(orchardId?: string) {
        let query = supabase.from('fleet_vehicles').select('*').order('name');
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data, error } = await query;
        if (error) throw error;
        return data || [];
    },

    async getVehicleNames(ids: string[]): Promise<Record<string, string>> {
        if (ids.length === 0) return {};
        const { data } = await supabase.from('fleet_vehicles')
            .select('id, name, driver_name').in('id', ids);
        return Object.fromEntries((data || []).map(v => [v.id, v.name]));
    },

    /* ── Transport Requests ── */
    async getPendingRequestStatuses(orchardId?: string) {
        let query = supabase.from('transport_requests')
            .select('status').in('status', ['pending', 'assigned']);
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data || [];
    },

    async getActiveRequests(orchardId?: string) {
        let query = supabase.from('transport_requests').select('*')
            .in('status', ['pending', 'assigned', 'in_progress'])
            .order('created_at', { ascending: false });
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data, error } = await query;
        if (error) throw error;
        return data || [];
    },

    async getCompletedRequests(orchardId?: string, limit = 50) {
        let query = supabase.from('transport_requests')
            .select('id, zone, bins_count, created_at, completed_at, assigned_vehicle, requester_name, notes, status')
            .in('status', ['completed', 'cancelled'])
            .order('completed_at', { ascending: false }).limit(limit);
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data, error } = await query;
        if (error) throw error;
        return data || [];
    },
};
