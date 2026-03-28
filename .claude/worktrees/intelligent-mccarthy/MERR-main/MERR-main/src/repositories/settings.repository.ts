/**
 * Settings Repository — Domain queries for harvest_settings table
 */
import { supabase } from '@/services/supabase';

export const settingsRepository = {
    /** Get harvest settings for an orchard */
    async getByOrchardId(orchardId: string) {
        const { data, error } = await supabase
            .from('harvest_settings')
            .select('*')
            .eq('orchard_id', orchardId)
            .single();

        if (error) return null;
        return data;
    },

    /** Upsert harvest settings for an orchard */
    async upsert(orchardId: string, updates: Record<string, unknown>): Promise<void> {
        const { error } = await supabase
            .from('harvest_settings')
            .upsert(
                { orchard_id: orchardId, ...updates },
                { onConflict: 'orchard_id' }
            );
        if (error) throw error;
    },
};
