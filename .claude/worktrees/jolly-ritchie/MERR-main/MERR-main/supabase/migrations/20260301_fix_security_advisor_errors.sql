-- =============================================
-- FIX SUPABASE SECURITY ADVISOR ERRORS (4 issues)
-- =============================================
-- Run this in the Supabase SQL Editor
-- 1. ENABLE RLS on public.alerts (has policies but RLS was disabled)
ALTER TABLE IF EXISTS public.alerts ENABLE ROW LEVEL SECURITY;
-- 2. ENABLE RLS on public.blocks (if it exists as a separate table)
ALTER TABLE IF EXISTS public.blocks ENABLE ROW LEVEL SECURITY;
-- Add basic RLS policy for blocks if none exists
DO $$ BEGIN IF EXISTS (
    SELECT 1
    FROM pg_tables
    WHERE tablename = 'blocks'
        AND schemaname = 'public'
) THEN -- Allow authenticated users to read blocks in their orchard
DROP POLICY IF EXISTS "Read blocks" ON public.blocks;
CREATE POLICY "Read blocks" ON public.blocks FOR
SELECT TO authenticated USING (orchard_id = get_my_orchard_id());
-- Allow managers to manage blocks
DROP POLICY IF EXISTS "Manage blocks" ON public.blocks;
CREATE POLICY "Manage blocks" ON public.blocks FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'admin')
    )
);
END IF;
END $$;
-- 3. Add RLS policies for alerts (if none exist)
DO $$ BEGIN IF EXISTS (
    SELECT 1
    FROM pg_tables
    WHERE tablename = 'alerts'
        AND schemaname = 'public'
) THEN -- Allow authenticated users to read alerts for their orchard
DROP POLICY IF EXISTS "Read alerts" ON public.alerts;
CREATE POLICY "Read alerts" ON public.alerts FOR
SELECT TO authenticated USING (orchard_id = get_my_orchard_id());
-- Allow system/managers to create alerts
DROP POLICY IF EXISTS "Create alerts" ON public.alerts;
CREATE POLICY "Create alerts" ON public.alerts FOR
INSERT WITH CHECK (true);
-- Allow managers to manage alerts
DROP POLICY IF EXISTS "Manage alerts" ON public.alerts;
CREATE POLICY "Manage alerts" ON public.alerts FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'admin')
    )
);
END IF;
END $$;
-- 4. FIX SECURITY DEFINER VIEW — recreate as SECURITY INVOKER
-- The view pickers_performance_today runs as the view owner, bypassing RLS
-- Recreate it without SECURITY DEFINER (SECURITY INVOKER is the default)
DROP VIEW IF EXISTS public.pickers_performance_today;
CREATE VIEW public.pickers_performance_today AS
SELECT p.id AS picker_id,
    p.name,
    p.orchard_id,
    p.team_leader_id,
    p.status,
    COALESCE(br_today.bucket_count, 0) AS total_buckets,
    br_today.last_scan
FROM public.pickers p
    LEFT JOIN LATERAL (
        SELECT COUNT(*) AS bucket_count,
            MAX(br.scanned_at) AS last_scan
        FROM public.bucket_records br
        WHERE br.picker_id = p.id
            AND br.deleted_at IS NULL
            AND DATE(br.scanned_at AT TIME ZONE 'Pacific/Auckland') = (CURRENT_DATE AT TIME ZONE 'Pacific/Auckland')::date
    ) br_today ON true
WHERE p.deleted_at IS NULL;
-- Grant SELECT to authenticated users
GRANT SELECT ON public.pickers_performance_today TO authenticated;
-- Verify fixes
SELECT 'Security fixes applied successfully' AS result;