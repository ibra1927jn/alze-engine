/**
 * Auth Hardening Repository — login_attempts + account_locks queries
 */
import { supabase } from '@/services/supabase';
import { nowNZST } from '@/utils/nzst';

export const authRepository = {
    /** Insert login attempt */
    async logAttempt(entry: Record<string, unknown>) {
        await supabase.from('login_attempts').insert(entry);
    },

    /** Get recent failed login attempts */
    async getRecentFailed(limit = 50) {
        const { data, error } = await supabase
            .from('login_attempts')
            .select('*')
            .eq('success', false)
            .order('attempt_time', { ascending: false })
            .limit(limit);
        if (error) throw error;
        return data || [];
    },

    /** Get active lock for email */
    async getActiveLock(email: string) {
        const { data } = await supabase
            .from('account_locks')
            .select('locked_until')
            .eq('email', email)
            .gt('locked_until', nowNZST())
            .is('unlocked_at', null)
            .order('locked_at', { ascending: false })
            .limit(1)
            .single();
        return data;
    },

    /** Get all current locks */
    async getCurrentLocks() {
        const { data, error } = await supabase
            .from('account_locks')
            .select('*')
            .gt('locked_until', nowNZST())
            .is('unlocked_at', null)
            .order('locked_at', { ascending: false });
        if (error) throw error;
        return data || [];
    },
};
