-- Migration: Add correction tracking columns to daily_attendance
-- Date: 2026-02-12
-- Description: Enables admin timesheet corrections with full audit trail.
--   When an admin corrects a check-in/check-out time, we record who did it,
--   when, and why.
ALTER TABLE daily_attendance
ADD COLUMN IF NOT EXISTS correction_reason TEXT,
    ADD COLUMN IF NOT EXISTS corrected_by UUID REFERENCES auth.users(id),
    ADD COLUMN IF NOT EXISTS corrected_at TIMESTAMPTZ;
-- Index for finding corrected records quickly
CREATE INDEX IF NOT EXISTS idx_daily_attendance_corrected ON daily_attendance (corrected_at)
WHERE corrected_at IS NOT NULL;
COMMENT ON COLUMN daily_attendance.correction_reason IS 'Reason for admin correction (e.g., "forgot to check out")';
COMMENT ON COLUMN daily_attendance.corrected_by IS 'UUID of the admin user who made the correction';
COMMENT ON COLUMN daily_attendance.corrected_at IS 'Timestamp when the correction was applied';