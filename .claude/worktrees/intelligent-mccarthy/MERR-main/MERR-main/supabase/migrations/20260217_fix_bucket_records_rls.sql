-- =============================================
-- 🔧 U5: Fix RLS table name mismatch
-- The app uses 'bucket_records' but RLS policies from
-- 20260211_complete_rls.sql target 'bucket_scans'.
-- This migration creates the correct policies on bucket_records.
-- =============================================
-- Enable RLS on the correct table
ALTER TABLE IF EXISTS bucket_records ENABLE ROW LEVEL SECURITY;
-- View policy: Managers see all, team leaders see their orchard
DROP POLICY IF EXISTS "bucket_records_view_policy" ON bucket_records;
CREATE POLICY "bucket_records_view_policy" ON bucket_records FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
        OR orchard_id IN (
            SELECT orchard_id
            FROM users
            WHERE id = auth.uid()
        )
    );
-- Insert policy: Managers, team leaders, and runners can insert
DROP POLICY IF EXISTS "bucket_records_insert_policy" ON bucket_records;
CREATE POLICY "bucket_records_insert_policy" ON bucket_records FOR
INSERT WITH CHECK (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role IN ('manager', 'team-leader', 'runner')
        )
    );
-- Update policy: Only managers can update (corrections)
DROP POLICY IF EXISTS "bucket_records_update_policy" ON bucket_records;
CREATE POLICY "bucket_records_update_policy" ON bucket_records FOR
UPDATE USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- Drop the stale bucket_scans policies if that table doesn't exist
-- (If bucket_scans does exist as a legacy table, leave its policies alone)
DO $$ BEGIN IF NOT EXISTS (
    SELECT 1
    FROM information_schema.tables
    WHERE table_schema = 'public'
        AND table_name = 'bucket_scans'
) THEN RAISE NOTICE 'bucket_scans table does not exist — no stale policies to clean up';
END IF;
END $$;