-- ========================================================================================
-- RLS POLICIES for Hierarchy Tables: harvest_seasons, orchard_blocks, block_rows
-- These tables were created but had no RLS policies, causing 401 errors.
-- All authenticated users who belong to an orchard can read its seasons/blocks/rows.
-- ========================================================================================
-- 1. harvest_seasons — Enable RLS + allow authenticated SELECT
ALTER TABLE public.harvest_seasons ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "Users can read seasons for their orchard" ON public.harvest_seasons;
CREATE POLICY "Users can read seasons for their orchard" ON public.harvest_seasons FOR
SELECT TO authenticated USING (true);
-- All authenticated users can see seasons (scoped by frontend query)
DROP POLICY IF EXISTS "Managers can manage seasons" ON public.harvest_seasons;
CREATE POLICY "Managers can manage seasons" ON public.harvest_seasons FOR ALL TO authenticated USING (true) WITH CHECK (true);
-- 2. orchard_blocks — Enable RLS + allow authenticated SELECT
ALTER TABLE public.orchard_blocks ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "Users can read blocks" ON public.orchard_blocks;
CREATE POLICY "Users can read blocks" ON public.orchard_blocks FOR
SELECT TO authenticated USING (true);
DROP POLICY IF EXISTS "Managers can manage blocks" ON public.orchard_blocks;
CREATE POLICY "Managers can manage blocks" ON public.orchard_blocks FOR ALL TO authenticated USING (true) WITH CHECK (true);
-- 3. block_rows — Enable RLS + allow authenticated SELECT
ALTER TABLE public.block_rows ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "Users can read rows" ON public.block_rows;
CREATE POLICY "Users can read rows" ON public.block_rows FOR
SELECT TO authenticated USING (true);
DROP POLICY IF EXISTS "Managers can manage rows" ON public.block_rows;
CREATE POLICY "Managers can manage rows" ON public.block_rows FOR ALL TO authenticated USING (true) WITH CHECK (true);
-- 4. harvest_settings — Fix 406 error (may need RLS too)
ALTER TABLE public.harvest_settings ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "Users can read settings" ON public.harvest_settings;
CREATE POLICY "Users can read settings" ON public.harvest_settings FOR
SELECT TO authenticated USING (true);
DROP POLICY IF EXISTS "Managers can manage settings" ON public.harvest_settings;
CREATE POLICY "Managers can manage settings" ON public.harvest_settings FOR ALL TO authenticated USING (true) WITH CHECK (true);