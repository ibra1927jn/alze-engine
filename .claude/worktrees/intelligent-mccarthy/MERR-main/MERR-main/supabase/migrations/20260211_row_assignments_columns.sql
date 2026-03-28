-- =============================================
-- ADD MISSING COLUMNS TO ROW_ASSIGNMENTS
-- Syncs DB schema with frontend RowAssignment interface
-- =============================================

-- Add 'side' column (north/south for orchard row sides)
ALTER TABLE public.row_assignments
  ADD COLUMN IF NOT EXISTS side TEXT DEFAULT 'north'
    CHECK (side IN ('north', 'south'));

-- Add 'assigned_pickers' array (UUID[] of picker IDs)
ALTER TABLE public.row_assignments
  ADD COLUMN IF NOT EXISTS assigned_pickers UUID[] DEFAULT '{}';

-- Add 'completion_percentage' (0-100)
ALTER TABLE public.row_assignments
  ADD COLUMN IF NOT EXISTS completion_percentage INTEGER DEFAULT 0
    CHECK (completion_percentage BETWEEN 0 AND 100);

-- Allow team_leaders to manage row assignments (not just managers)
DROP POLICY IF EXISTS "Team leaders manage row assignments" ON public.row_assignments;
CREATE POLICY "Team leaders manage row assignments" ON public.row_assignments
  FOR ALL USING (
    EXISTS (
      SELECT 1 FROM public.users
      WHERE id = auth.uid() AND role IN ('manager', 'admin', 'team_leader')
    )
  );
