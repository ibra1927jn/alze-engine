/**
 * Admin Repository — Domain queries for admin operations
 * Provides cross-orchard management capabilities.
 */
import { supabase } from '@/services/supabase';
import type { OrchardOverview, UserRecord } from '@/services/admin.service';

export const adminRepository = {
    /** Get all orchards ordered by name */
    async getAllOrchards(): Promise<OrchardOverview[]> {
        const { data, error } = await supabase
            .from('orchards')
            .select('id, name, total_rows')
            .order('name');

        if (error) throw error;
        return (data || []).map(o => ({
            id: o.id,
            name: o.name,
            total_rows: o.total_rows || 0,
            active_pickers: 0,
            today_buckets: 0,
            compliance_score: 100,
        }));
    },

    /** Get all users with optional filters */
    async getAllUsers(filters?: {
        role?: string;
        orchardId?: string;
        search?: string;
    }): Promise<UserRecord[]> {
        let query = supabase
            .from('users')
            .select('id, email, full_name, role, is_active, orchard_id, created_at')
            .order('full_name');

        if (filters?.role) query = query.eq('role', filters.role);
        if (filters?.orchardId) query = query.eq('orchard_id', filters.orchardId);
        if (filters?.search) {
            query = query.or(`full_name.ilike.%${filters.search}%,email.ilike.%${filters.search}%`);
        }

        const { data, error } = await query;
        if (error) throw error;
        return (data || []) as UserRecord[];
    },

    /** Update a user's role */
    async updateUserRole(userId: string, newRole: string): Promise<void> {
        const { error } = await supabase
            .from('users')
            .update({ role: newRole, updated_at: new Date().toISOString() })
            .eq('id', userId);
        if (error) throw error;
    },

    /** Deactivate a user (soft-delete) */
    async deactivateUser(userId: string): Promise<void> {
        const { error } = await supabase
            .from('users')
            .update({ is_active: false, updated_at: new Date().toISOString() })
            .eq('id', userId);
        if (error) throw error;
    },

    /** Reactivate a user */
    async reactivateUser(userId: string): Promise<void> {
        const { error } = await supabase
            .from('users')
            .update({ is_active: true, updated_at: new Date().toISOString() })
            .eq('id', userId);
        if (error) throw error;
    },
};
