/**
 * Sticker Repository — Domain queries for scanned_stickers table
 */
import { supabase } from '@/services/supabase';

export const stickerRepository = {
    /** Check if a sticker code exists */
    async findByCode(stickerCode: string) {
        const { data, error } = await supabase
            .from('scanned_stickers')
            .select('id')
            .eq('sticker_code', stickerCode)
            .maybeSingle();
        if (error) throw error;
        return data;
    },

    /** Insert a new sticker scan */
    async insert(payload: {
        sticker_code: string;
        picker_id: string;
        bin_id: string;
        scanned_by?: string;
        team_leader_id?: string;
        orchard_id?: string;
    }) {
        const { data, error } = await supabase
            .from('scanned_stickers')
            .insert([payload])
            .select()
            .single();
        return { data, error };
    },

    /** Count stickers by team leader (total) */
    async countByTeamLeader(teamLeaderId: string): Promise<number> {
        const { count } = await supabase
            .from('scanned_stickers')
            .select('*', { count: 'exact', head: true })
            .eq('team_leader_id', teamLeaderId);
        return count || 0;
    },

    /** Count stickers by team leader in date range */
    async countByTeamLeaderInRange(teamLeaderId: string, startISO: string, endISO: string): Promise<number> {
        const { count } = await supabase
            .from('scanned_stickers')
            .select('*', { count: 'exact', head: true })
            .eq('team_leader_id', teamLeaderId)
            .gte('scanned_at', startISO)
            .lte('scanned_at', endISO);
        return count || 0;
    },

    /** Count stickers by picker in date range */
    async countByPickerInRange(pickerId: string, startISO: string, endISO: string): Promise<number> {
        const { count } = await supabase
            .from('scanned_stickers')
            .select('*', { count: 'exact', head: true })
            .eq('picker_id', pickerId)
            .gte('scanned_at', startISO)
            .lte('scanned_at', endISO);
        return count || 0;
    },
};
