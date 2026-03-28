/**
 * Timesheet utilities — pure helper functions for attendance time formatting
 * Extracted from TimesheetEditor.tsx
 * @module views/manager/timesheet-utils
 */
import { logger } from '@/utils/logger';

/** Attendance record row shape */
export interface AttendanceRow {
    id: string;
    picker_id: string;
    check_in_time: string | null;
    check_out_time: string | null;
    status: string;
    date: string;
    correction_reason?: string;
    corrected_at?: string;
    picker?: {
        id: string;
        name: string;
        picker_id: string;
    };
}

/** Format ISO timestamp to HH:MM for input fields */
export function formatTimeForInput(isoString: string): string {
    try {
        const d = new Date(isoString);
        return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`;
    } catch (e) {
        logger.warn('[TimesheetEditor] Invalid date for input formatting:', isoString, e);
        return '';
    }
}

/** Format ISO timestamp to localized HH:MM display */
export function formatTime(isoString: string | null): string {
    if (!isoString) return '—';
    try {
        const d = new Date(isoString);
        return d.toLocaleTimeString('en-NZ', { hour: '2-digit', minute: '2-digit', hour12: false });
    } catch (e) {
        logger.warn('[TimesheetEditor] Invalid date for display formatting:', isoString, e);
        return '—';
    }
}

/** Calculate hours between check-in and check-out */
export function calculateHours(checkIn: string | null, checkOut: string | null): number | null {
    if (!checkIn) return null;
    const start = new Date(checkIn);
    const end = checkOut ? new Date(checkOut) : new Date();
    return Math.round((end.getTime() - start.getTime()) / (1000 * 60 * 60) * 10) / 10;
}

/** Flag abnormal hour counts (>12 or <0) */
export function isAbnormal(hours: number | null): boolean {
    return hours !== null && (hours > 12 || hours < 0);
}
