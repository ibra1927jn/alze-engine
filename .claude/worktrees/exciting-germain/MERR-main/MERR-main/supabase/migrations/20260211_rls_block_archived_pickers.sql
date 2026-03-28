-- =============================================
-- HARVESTPRO NZ - BLOCK BUCKET SCANS FROM ARCHIVED PICKERS
-- Migration: 20260211_rls_block_archived_pickers
-- Created: 2026-02-11
-- Purpose: Prevent offline devices from syncing buckets for pickers that have been removed/suspended
-- =============================================

-- 1. Add policy to bucket_records table
-- This blocks any attempt to insert bucket records for pickers with status='archived'
CREATE POLICY "Block scans from archived pickers" 
ON public.bucket_records
FOR INSERT
WITH CHECK (
    NOT EXISTS (
        SELECT 1 FROM public.pickers p
        WHERE p.id = bucket_records.picker_id
        AND p.status = 'archived'
    )
);

-- 2. Add same policy to bucket_events table (if it exists and is different from bucket_records)
-- Check if bucket_events table exists first
DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = 'public' AND table_name = 'bucket_events') THEN
        EXECUTE 'CREATE POLICY "Block events from archived pickers" 
                ON public.bucket_events
                FOR INSERT
                WITH CHECK (
                    NOT EXISTS (
                        SELECT 1 FROM public.pickers p
                        WHERE p.id = bucket_events.picker_id
                        AND p.status = ''archived''
                    )
                )';
    END IF;
END $$;

-- 3. Add performance index for archived pickers lookups
-- This index accelerates the RLS policy checks
CREATE INDEX IF NOT EXISTS idx_pickers_status_id 
ON public.pickers(status, id) 
WHERE status = 'archived';

-- 4. Add index for picker status to support filtering
CREATE INDEX IF NOT EXISTS idx_pickers_status 
ON public.pickers(status);

-- =============================================
-- VERIFICATION QUERIES
-- =============================================

-- Verify policies were created
-- SELECT policyname, tablename, cmd, qual 
-- FROM pg_policies 
-- WHERE tablename IN ('bucket_records', 'bucket_events')
-- AND policyname LIKE '%archived%';

-- Verify indexes were created
-- SELECT indexname, tablename 
-- FROM pg_indexes 
-- WHERE tablename = 'pickers' 
-- AND indexname LIKE '%status%';

-- Test policy (should fail if uncommented and picker is archived)
-- INSERT INTO bucket_records (picker_id, orchard_id, scanned_by)
-- VALUES ('[archived-picker-uuid]', '[orchard-uuid]', '[user-uuid]');
