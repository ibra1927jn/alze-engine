/**
 * Row Assignment Repository — persistence for row_assignments + picker current_row
 */
import { supabase } from '@/services/supabase';

export const rowRepository = {
    /** Bulk update picker current_row */
    async updatePickerRows(pickerIds: string[], row: number) {
        const { error } = await supabase.from('pickers')
            .update({ current_row: row }).in('id', pickerIds);
        return { error };
    },

    /** Update row assignment progress */
    async updateProgress(rowId: string, percentage: number) {
        const { error } = await supabase.from('row_assignments')
            .update({ completion_percentage: percentage }).eq('id', rowId);
        if (error) throw error;
    },

    /** Complete a row assignment */
    async completeRow(rowId: string) {
        const { error } = await supabase.from('row_assignments')
            .update({ completion_percentage: 100, status: 'completed' }).eq('id', rowId);
        if (error) throw error;
    },
};
