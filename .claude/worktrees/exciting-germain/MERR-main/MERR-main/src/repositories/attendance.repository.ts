/**
 * Attendance Repository — Domain queries for daily_attendance table
 *
 * Encapsulates all Supabase queries related to attendance so that
 * attendance.service.ts has ZERO direct supabase imports.
 */
import { supabase } from '@/services/supabase';
import { logger } from '@/utils/logger';

export const attendanceRepository = {
    /** Get today's attendance with picker info (join) */
    async getDailyWithPickers(orchardId: string, date: string) {
        const { data, error } = await supabase
            .from('daily_attendance')
            .select(`*, picker:pickers ( name, role, team_leader_id )`)
            .eq('orchard_id', orchardId)
            .eq('date', date);
        if (error) throw error;
        return data || [];
    },

    /** Get attendance by date with picker join (id, name, picker_id) */
    async getByDateWithPickers(orchardId: string, date: string) {
        const { data, error } = await supabase
            .from('daily_attendance')
            .select(`*, picker:pickers ( id, name, picker_id )`)
            .eq('orchard_id', orchardId)
            .eq('date', date)
            .order('check_in_time', { ascending: true });
        if (error) throw error;
        return data || [];
    },

    /** Find a single attendance record */
    async findOne(pickerId: string, orchardId: string, date: string) {
        const { data } = await supabase
            .from('daily_attendance')
            .select('id')
            .eq('picker_id', pickerId)
            .eq('orchard_id', orchardId)
            .eq('date', date)
            .maybeSingle();
        return data;
    },

    /** Insert new attendance record */
    async insert(record: Record<string, unknown>) {
        const { data, error } = await supabase
            .from('daily_attendance')
            .insert(record)
            .select()
            .single();
        if (error) throw error;
        return data;
    },

    /** Update attendance record */
    async update(id: string, updates: Record<string, unknown>) {
        const { data, error } = await supabase
            .from('daily_attendance')
            .update(updates)
            .eq('id', id)
            .select()
            .single();
        if (error) throw error;
        return data;
    },

    /** Fetch check_in_time for a single attendance record */
    async getCheckInTime(id: string) {
        const { data } = await supabase
            .from('daily_attendance')
            .select('check_in_time')
            .eq('id', id)
            .single();
        return data;
    },

    /** Fetch check_in/out for a single record */
    async getCheckTimes(id: string) {
        const { data } = await supabase
            .from('daily_attendance')
            .select('check_in_time, check_out_time')
            .eq('id', id)
            .single();
        return data;
    },

    /** Get active pickers with inner join on pickers table */
    async getActivePickers(orchardId: string, date: string) {
        const { data, error } = await supabase
            .from('daily_attendance')
            .select(`picker_id, status, check_in_time, pickers!inner ( * )`)
            .eq('orchard_id', orchardId)
            .eq('date', date)
            .eq('status', 'present');
        if (error) throw error;
        return data || [];
    },

    /** Get hours summary for period (for payroll calculations) */
    async getHoursSummary(orchardId: string | undefined, sinceDate: string) {
        let query = supabase
            .from('daily_attendance')
            .select('picker_id, check_in_time, check_out_time')
            .gte('date', sinceDate);
        if (orchardId) query = query.eq('orchard_id', orchardId);
        const { data } = await query;
        return data || [];
    },

    /** Get attendance for timesheet export (payroll.service) */
    async getTimesheets(orchardId: string, date: string) {
        const { data, error } = await supabase
            .from('daily_attendance')
            .select('id, picker_id, date, check_in_time, check_out_time, verified_by, orchard_id, updated_at')
            .eq('orchard_id', orchardId)
            .eq('date', date)
            .order('check_in_time', { ascending: true });
        if (error) {
            logger.error('[AttendanceRepo] Error fetching timesheets:', error);
            return [];
        }
        return data || [];
    },
};
