import { logger } from '@/utils/logger';
import { nowNZST, todayNZST } from '@/utils/nzst';
import type { SupabasePicker, SupabasePerformanceStat } from '../types/database.types';
import { attendanceRepository } from '@/repositories/attendance.repository';
import { pickerRepository } from '@/repositories/picker.repository';
import { auditRepository } from '@/repositories/audit.repository';
import { rpcRepository } from '@/repositories/rpc.repository';

export const attendanceService = {
    // --- DAILY ATTENDANCE (LIVE OPERATIONS) ---

    async getDailyAttendance(orchardId: string, date?: string) {
        const queryDate = date || todayNZST();
        return attendanceRepository.getDailyWithPickers(orchardId, queryDate);
    },

    async checkInPicker(pickerId: string, orchardId: string, verifiedBy?: string) {
        const today = todayNZST();

        // ── Attempt atomic RPC ──
        const { data: rpcResult, error: rpcErr } = await rpcRepository.call('check_in_picker', {
            p_picker_id: pickerId,
            p_orchard_id: orchardId,
            p_verified_by: verifiedBy || null,
        });

        if (!rpcErr && rpcResult) {
            return rpcResult as { picker_id: string; status: string; id: string };
        }

        if (rpcErr && rpcErr.code !== '42883') throw rpcErr;

        // ── Sequential fallback ──
        const existing = await attendanceRepository.findOne(pickerId, orchardId, today);

        if (existing) {
            await pickerRepository.updateStatus(pickerId, 'active');
            return { picker_id: pickerId, status: 'present', id: existing.id };
        }

        const data = await attendanceRepository.insert({
            picker_id: pickerId,
            orchard_id: orchardId,
            date: today,
            check_in_time: nowNZST(),
            status: 'present',
            verified_by: verifiedBy
        });

        if (data) {
            await pickerRepository.updateStatus(pickerId, 'active');
        }
        return data;
    },

    async checkOutPicker(attendanceId: string) {
        // ── Attempt atomic RPC ──
        const { data: rpcResult, error: rpcErr } = await rpcRepository.call('check_out_picker', {
            p_attendance_id: attendanceId,
        });

        if (!rpcErr && rpcResult) {
            return rpcResult as { id: string; picker_id: string; check_out_time: string; hours_worked: number };
        }

        if (rpcErr && rpcErr.code !== '42883') throw rpcErr;

        // ── Sequential fallback ──
        const existing = await attendanceRepository.getCheckInTime(attendanceId);

        const checkOutTime = nowNZST();
        let hoursWorked: number | undefined;
        if (existing?.check_in_time) {
            hoursWorked = Math.round(
                ((new Date(checkOutTime).getTime() - new Date(existing.check_in_time).getTime()) / 3600000) * 100
            ) / 100;
        }

        const data = await attendanceRepository.update(attendanceId, {
            check_out_time: checkOutTime,
            status: 'present',
            ...(hoursWorked !== undefined ? { hours_worked: hoursWorked } : {}),
        });

        if (data) {
            await pickerRepository.updateStatus(data.picker_id, 'inactive');
        }
        return data;
    },

    async getTodayPerformance(orchardId?: string) {
        return pickerRepository.getPerformanceToday(orchardId);
    },

    // 4. Get Active Pickers for Live Ops (Runner View)
    async getActivePickersForLiveOps(orchardId: string) {
        const today = todayNZST();

        const attendanceData = await attendanceRepository.getActivePickers(orchardId, today);
        const perfData = await pickerRepository.getPerformanceToday(orchardId);

        return (attendanceData || []).map((record: unknown) => {
            const rec = record as Record<string, unknown>;
            const p = rec.pickers as SupabasePicker;
            const perf = perfData?.find((stat: SupabasePerformanceStat) => stat.picker_id === p.id);

            const checkInTime = rec.check_in_time as string | null;
            let hoursWorked = 0;
            if (checkInTime) {
                hoursWorked = Math.round(
                    ((Date.now() - new Date(checkInTime).getTime()) / 3600000) * 100
                ) / 100;
                hoursWorked = Math.max(0, hoursWorked);
            }

            return {
                id: p.id,
                picker_id: p.picker_id || p.id,
                name: p.name || 'Unknown',
                avatar: (p.name || '??').substring(0, 2).toUpperCase(),
                hours: hoursWorked,
                total_buckets_today: perf?.total_buckets || 0,
                current_row: p.current_row || 0,
                status: (p.status !== 'archived' && p.status !== 'inactive' ? 'active' : 'inactive') as 'active' | 'break' | 'issue',
                safety_verified: p.safety_verified,
                qcStatus: [1, 1, 1],
                harness_id: p.picker_id || undefined,
                team_leader_id: p.team_leader_id || undefined,
                orchard_id: p.orchard_id,
                role: 'picker'
            };
        });
    },

    // ========================================
    // ADMIN: TIMESHEET CORRECTIONS
    // ========================================

    async getAttendanceByDate(orchardId: string, date: string) {
        return attendanceRepository.getByDateWithPickers(orchardId, date);
    },

    async correctAttendance(
        attendanceId: string,
        updates: { check_in_time?: string; check_out_time?: string },
        reason: string,
        adminId: string
    ): Promise<void> {
        // ── Attempt atomic RPC ──
        const { error: rpcErr } = await rpcRepository.call('correct_attendance', {
            p_attendance_id: attendanceId,
            p_check_in_time: updates.check_in_time || null,
            p_check_out_time: updates.check_out_time || null,
            p_reason: reason,
            p_admin_id: adminId,
        });

        if (!rpcErr) return;
        if (rpcErr.code !== '42883') throw rpcErr;

        // ── Sequential fallback ──
        const updatePayload: Record<string, unknown> = {
            ...updates,
            correction_reason: reason,
            corrected_by: adminId,
            corrected_at: nowNZST(),
        };

        const checkIn = updates.check_in_time;
        const checkOut = updates.check_out_time;
        if (checkIn && checkOut) {
            updatePayload.hours_worked = Math.max(0, Math.round(
                ((new Date(checkOut).getTime() - new Date(checkIn).getTime()) / 3600000) * 100
            ) / 100);
        } else if (checkIn || checkOut) {
            const existing = await attendanceRepository.getCheckTimes(attendanceId);
            if (existing) {
                const ci = checkIn || existing.check_in_time;
                const co = checkOut || existing.check_out_time;
                if (ci && co) {
                    updatePayload.hours_worked = Math.max(0, Math.round(
                        ((new Date(co).getTime() - new Date(ci).getTime()) / 3600000) * 100
                    ) / 100);
                }
            }
        }

        await attendanceRepository.update(attendanceId, updatePayload);

        // Audit log (best-effort in fallback mode)
        await auditRepository.insertSafe({
            action: 'timesheet_correction',
            entity_type: 'daily_attendance',
            entity_id: attendanceId,
            performed_by: adminId,
            new_values: updates,
            notes: reason,
        });
    },
};