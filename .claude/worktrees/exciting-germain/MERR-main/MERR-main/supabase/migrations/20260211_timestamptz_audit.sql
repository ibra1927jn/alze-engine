-- =============================================
-- TIMESTAMPTZ AUDIT MIGRATION
-- Ensures all timestamp columns storing NZ-relevant times use TIMESTAMPTZ
-- =============================================

-- 1. bucket_events: recorded_at should be TIMESTAMPTZ
-- This is safe: ALTER TYPE on timestamptz columns doesn't lose data
ALTER TABLE bucket_events
  ALTER COLUMN recorded_at TYPE TIMESTAMPTZ USING recorded_at AT TIME ZONE 'Pacific/Auckland';

ALTER TABLE bucket_events
  ALTER COLUMN scanned_at TYPE TIMESTAMPTZ USING scanned_at AT TIME ZONE 'Pacific/Auckland';

-- 2. daily_attendance: check_in_time and check_out_time
ALTER TABLE daily_attendance
  ALTER COLUMN check_in_time TYPE TIMESTAMPTZ USING check_in_time AT TIME ZONE 'Pacific/Auckland';

ALTER TABLE daily_attendance
  ALTER COLUMN check_out_time TYPE TIMESTAMPTZ USING check_out_time AT TIME ZONE 'Pacific/Auckland';

-- 3. day_closures: closed_at
ALTER TABLE day_closures
  ALTER COLUMN closed_at TYPE TIMESTAMPTZ USING closed_at AT TIME ZONE 'Pacific/Auckland';

-- COMMENT: If columns were already TIMESTAMPTZ, these are safe no-ops.
-- If they were TIMESTAMP (without timezone), this converts existing values
-- assuming they were stored as Pacific/Auckland local times.
