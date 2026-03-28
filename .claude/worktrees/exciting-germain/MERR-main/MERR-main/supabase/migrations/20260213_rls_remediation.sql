-- =============================================
-- RLS REMEDIATION — Fix 3 gaps found in audit
-- Date: 2026-02-13
-- Gaps:
--   1. day_closures: policies exist but RLS never enabled
--   2. bins: RLS enabled but zero policies defined
--   3. qc_inspections: only in scripts/, not in migrations
-- =============================================
-- =============================================
-- 1. DAY_CLOSURES — Enable RLS (policies already exist)
-- =============================================
ALTER TABLE public.day_closures ENABLE ROW LEVEL SECURITY;
-- =============================================
-- 2. BINS — Add missing policies
-- =============================================
-- bins already has ENABLE ROW LEVEL SECURITY from schema_v1
-- but zero policies → all queries return empty by default
-- Read: same orchard members can view bins
DROP POLICY IF EXISTS "Read bins" ON public.bins;
CREATE POLICY "Read bins" ON public.bins FOR
SELECT USING (orchard_id = get_my_orchard_id());
-- Manage: managers and team leaders can insert/update/delete
DROP POLICY IF EXISTS "Manage bins" ON public.bins;
CREATE POLICY "Manage bins" ON public.bins FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_manager_or_leader()
);
-- Insert: runners can insert bins (field scanning)
DROP POLICY IF EXISTS "Runners insert bins" ON public.bins;
CREATE POLICY "Runners insert bins" ON public.bins FOR
INSERT WITH CHECK (orchard_id = get_my_orchard_id());
-- =============================================
-- 3. QC_INSPECTIONS — Create table + RLS if not exists
-- =============================================
CREATE TABLE IF NOT EXISTS public.qc_inspections (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    picker_id UUID NOT NULL REFERENCES public.users(id) ON DELETE CASCADE,
    inspector_id UUID NOT NULL REFERENCES public.users(id) ON DELETE CASCADE,
    grade TEXT NOT NULL CHECK (grade IN ('A', 'B', 'C', 'reject')),
    notes TEXT,
    photo_url TEXT,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- Indexes
CREATE INDEX IF NOT EXISTS idx_qc_inspections_orchard_date ON public.qc_inspections (orchard_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_qc_inspections_picker ON public.qc_inspections (picker_id, created_at DESC);
-- Enable RLS
ALTER TABLE public.qc_inspections ENABLE ROW LEVEL SECURITY;
-- Read: same orchard can view inspections
DROP POLICY IF EXISTS "Read qc inspections" ON public.qc_inspections;
CREATE POLICY "Read qc inspections" ON public.qc_inspections FOR
SELECT USING (orchard_id = get_my_orchard_id());
-- Insert: QC inspectors can create inspections
DROP POLICY IF EXISTS "QC inspectors create inspections" ON public.qc_inspections;
CREATE POLICY "QC inspectors create inspections" ON public.qc_inspections FOR
INSERT WITH CHECK (
        inspector_id = auth.uid()
        AND orchard_id = get_my_orchard_id()
    );
-- Manage: managers can update/delete inspections
DROP POLICY IF EXISTS "Managers manage inspections" ON public.qc_inspections;
CREATE POLICY "Managers manage inspections" ON public.qc_inspections FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'qc_inspector', 'admin')
    )
);
-- =============================================
-- 4. VERIFICATION
-- =============================================
-- Verify all tables have RLS enabled
DO $$
DECLARE t RECORD;
missing_rls TEXT := '';
BEGIN FOR t IN
SELECT tablename
FROM pg_tables
WHERE schemaname = 'public'
    AND NOT rowsecurity LOOP missing_rls := missing_rls || t.tablename || ', ';
END LOOP;
IF missing_rls = '' THEN RAISE NOTICE 'RLS REMEDIATION COMPLETE: All public tables have RLS enabled';
ELSE RAISE WARNING 'Tables still missing RLS: %',
missing_rls;
END IF;
END $$;
SELECT 'RLS remediation migration applied successfully' AS result;