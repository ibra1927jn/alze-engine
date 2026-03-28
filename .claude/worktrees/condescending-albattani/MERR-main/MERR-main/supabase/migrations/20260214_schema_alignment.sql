-- Migration: Align DB schema with TypeScript types
-- Date: 2026-02-14
-- Description: Fixes column mismatches found during pessimistic audit
--   1. Add total_rows to orchards (TS expects it, only total_blocks exists)
--   2. Add quality_grade to bucket_records (only existed on quality_inspections)
--   3. Add 'in-progress' to bins.status CHECK (TS uses it but DB only allows 'partial')
-- ─── 1. orchards.total_rows ───────────────────────────────────────
ALTER TABLE public.orchards
ADD COLUMN IF NOT EXISTS total_rows INTEGER DEFAULT 20;
COMMENT ON COLUMN public.orchards.total_rows IS 'Number of rows in the orchard. Used by HeatMap row validation and row assignments.';
-- ─── 2. bucket_records.quality_grade ──────────────────────────────
ALTER TABLE public.bucket_records
ADD COLUMN IF NOT EXISTS quality_grade TEXT;
COMMENT ON COLUMN public.bucket_records.quality_grade IS 'Optional quality grade assigned during scanning (A/B/C/reject).';
-- ─── 3. bins.status — allow in-progress alongside partial ────────
-- Drop old constraint, re-create with both values
DO $$ BEGIN IF EXISTS (
    SELECT 1
    FROM information_schema.table_constraints
    WHERE table_name = 'bins'
        AND constraint_type = 'CHECK'
        AND constraint_name = 'bins_status_check'
) THEN
ALTER TABLE bins DROP CONSTRAINT bins_status_check;
END IF;
ALTER TABLE bins
ADD CONSTRAINT bins_status_check CHECK (
        status IN (
            'empty',
            'partial',
            'in-progress',
            'full',
            'collected'
        )
    );
RAISE NOTICE 'bins.status CHECK updated: added in-progress';
END $$;