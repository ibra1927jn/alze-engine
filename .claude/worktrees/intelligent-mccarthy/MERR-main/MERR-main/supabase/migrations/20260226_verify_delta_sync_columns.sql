-- ========================================================================================
-- HARVESTPRO NZ - VERIFICATION MIGRATION: Ensure columns exist for Delta Sync
-- Run this ONLY if your Supabase DB is missing updated_at/deleted_at/version columns.
-- All statements are idempotent (IF NOT EXISTS / IF NOT EXISTS).
-- ========================================================================================
-- FASE 1: Verify hierarchy tables exist (should already be there from V3)
CREATE TABLE IF NOT EXISTS public.harvest_seasons (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    start_date DATE NOT NULL,
    end_date DATE,
    status TEXT DEFAULT 'active' CHECK (status IN ('active', 'closed', 'archived')),
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now()
);
CREATE TABLE IF NOT EXISTS public.orchard_blocks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    season_id UUID NOT NULL REFERENCES public.harvest_seasons(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    total_rows INTEGER NOT NULL DEFAULT 0,
    start_row INTEGER NOT NULL DEFAULT 1,
    color_code TEXT DEFAULT '#dc2626',
    status TEXT DEFAULT 'idle' CHECK (
        status IN ('idle', 'active', 'complete', 'alert')
    ),
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_active_block ON public.orchard_blocks (orchard_id, season_id, name)
WHERE deleted_at IS NULL;
CREATE TABLE IF NOT EXISTS public.block_rows (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    block_id UUID NOT NULL REFERENCES public.orchard_blocks(id) ON DELETE CASCADE,
    row_number INTEGER NOT NULL,
    variety TEXT,
    target_buckets INTEGER DEFAULT 100,
    side TEXT CHECK (side IN ('north', 'south', 'both')),
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_active_row ON public.block_rows (block_id, row_number)
WHERE deleted_at IS NULL;
-- FASE 2: Ensure delta sync columns exist on all critical tables
ALTER TABLE public.bucket_records
ADD COLUMN IF NOT EXISTS row_id UUID REFERENCES public.block_rows(id),
    ADD COLUMN IF NOT EXISTS season_id UUID REFERENCES public.harvest_seasons(id),
    ADD COLUMN IF NOT EXISTS deleted_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ DEFAULT now(),
    ADD COLUMN IF NOT EXISTS version INTEGER DEFAULT 1;
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS season_id UUID REFERENCES public.harvest_seasons(id),
    ADD COLUMN IF NOT EXISTS deleted_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ DEFAULT now(),
    ADD COLUMN IF NOT EXISTS version INTEGER DEFAULT 1;
ALTER TABLE public.pickers
ADD COLUMN IF NOT EXISTS role TEXT DEFAULT 'picker',
    ADD COLUMN IF NOT EXISTS deleted_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ DEFAULT now(),
    ADD COLUMN IF NOT EXISTS version INTEGER DEFAULT 1;
ALTER TABLE public.bins
ADD COLUMN IF NOT EXISTS deleted_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ DEFAULT now(),
    ADD COLUMN IF NOT EXISTS block_id UUID REFERENCES public.orchard_blocks(id);
-- FASE 3: Delta sync indexes
CREATE INDEX IF NOT EXISTS idx_sync_pickers ON public.pickers (updated_at)
WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_sync_buckets ON public.bucket_records (updated_at)
WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_sync_attendance ON public.daily_attendance (updated_at)
WHERE deleted_at IS NULL;
-- FASE 4: Triggers for optimistic locking + auto updated_at
CREATE OR REPLACE FUNCTION public.bump_version_and_update_time() RETURNS TRIGGER AS $$ BEGIN IF OLD.version IS DISTINCT
FROM NEW.version THEN RAISE EXCEPTION 'Conflict: record modified by another user (expected v%, got v%)',
    NEW.version,
    OLD.version;
END IF;
NEW.version = COALESCE(OLD.version, 0) + 1;
NEW.updated_at = now();
RETURN NEW;
END;
$$ LANGUAGE plpgsql;
CREATE OR REPLACE FUNCTION public.auto_update_updated_at() RETURNS TRIGGER AS $$ BEGIN NEW.updated_at = now();
RETURN NEW;
END;
$$ LANGUAGE plpgsql;
DROP TRIGGER IF EXISTS trg_bump_version_pickers ON public.pickers;
CREATE TRIGGER trg_bump_version_pickers BEFORE
UPDATE ON public.pickers FOR EACH ROW EXECUTE FUNCTION public.bump_version_and_update_time();
DROP TRIGGER IF EXISTS trg_bump_version_buckets ON public.bucket_records;
CREATE TRIGGER trg_bump_version_buckets BEFORE
UPDATE ON public.bucket_records FOR EACH ROW EXECUTE FUNCTION public.bump_version_and_update_time();
DROP TRIGGER IF EXISTS trg_bump_version_attendance ON public.daily_attendance;
CREATE TRIGGER trg_bump_version_attendance BEFORE
UPDATE ON public.daily_attendance FOR EACH ROW EXECUTE FUNCTION public.bump_version_and_update_time();
DROP TRIGGER IF EXISTS trg_auto_time_bins ON public.bins;
CREATE TRIGGER trg_auto_time_bins BEFORE
UPDATE ON public.bins FOR EACH ROW EXECUTE FUNCTION public.auto_update_updated_at();
-- FASE 5: Assign orphan records to default season
DO $$
DECLARE default_orchard_id UUID;
default_season_id UUID;
BEGIN
SELECT id INTO default_orchard_id
FROM public.orchards
LIMIT 1;
IF default_orchard_id IS NOT NULL THEN
SELECT id INTO default_season_id
FROM public.harvest_seasons
WHERE orchard_id = default_orchard_id
LIMIT 1;
IF default_season_id IS NULL THEN
INSERT INTO public.harvest_seasons (orchard_id, name, start_date)
VALUES (default_orchard_id, 'Season 2026', '2026-01-01')
RETURNING id INTO default_season_id;
END IF;
UPDATE public.bucket_records
SET season_id = default_season_id
WHERE season_id IS NULL;
UPDATE public.daily_attendance
SET season_id = default_season_id
WHERE season_id IS NULL;
END IF;
END $$;