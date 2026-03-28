/**
 * features/attendance — Domain barrel export
 * 
 * Check-in/check-out, daily attendance, timesheet corrections.
 */

// Services
export { attendanceService } from '@/services/attendance.service';

// Hooks
export { useAttendance } from '@/hooks/useAttendance';

// Types
export type { ValidatedAttendance } from '@/lib/schemas';
export { AttendanceRecordSchema } from '@/lib/schemas';
