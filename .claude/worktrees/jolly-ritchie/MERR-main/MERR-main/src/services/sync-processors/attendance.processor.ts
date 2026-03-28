/**
 * Attendance Processor — Strategy Pattern processor for ATTENDANCE sync items.
 *
 * Handles check-in (append with dedup) and check-out (optimistic lock).
 * Check-out operations use withOptimisticLock to prevent silent overwrites
 * when a Manager corrects attendance while a Team Leader is offline.
 */

import { attendanceService } from '../attendance.service';
import { withOptimisticLock } from '../optimistic-lock.service';
import { supabase } from '../supabase';
import type { AttendancePayload } from './types';

export async function processAttendance(
    payload: AttendancePayload,
    expectedUpdatedAt?: string
): Promise<void> {
    if (payload.check_out_time && payload.attendanceId && expectedUpdatedAt) {
        // 🔧 L25: Calculate hours_worked for offline checkout
        // Without this, syncing offline checkouts left hours_worked as null → payroll = $0
        let hoursWorked: number | undefined;
        const { data: existing } = await supabase
            .from('daily_attendance')
            .select('check_in_time')
            .eq('id', payload.attendanceId)
            .single();

        if (existing?.check_in_time && payload.check_out_time) {
            // 🔧 U4: Math.max(0, ...) prevents negative hours from admin typos
            hoursWorked = Math.max(0, Math.round(
                ((new Date(payload.check_out_time).getTime() - new Date(existing.check_in_time).getTime()) / 3600000) * 100
            ) / 100);
        }

        // Check-out with optimistic lock — prevents silent overwrite
        // if a Manager has corrected attendance while TL was offline
        const result = await withOptimisticLock({
            table: 'daily_attendance',
            recordId: payload.attendanceId,
            expectedUpdatedAt,
            updates: {
                check_out_time: payload.check_out_time,
                status: 'present',
                ...(hoursWorked !== undefined ? { hours_worked: hoursWorked } : {}),
            }
        });
        if (!result.success) {
            throw new Error(
                `Optimistic lock conflict on attendance ${payload.attendanceId}: ` +
                `expected=${expectedUpdatedAt}, server has newer version`
            );
        }
    } else {
        // Check-in: delegate to existing service (already has dedup logic)
        await attendanceService.checkInPicker(
            payload.picker_id,
            payload.orchard_id,
            payload.verifiedBy
        );
    }
}
