-- SEED: Blocks & Rows for the REAL orchard in Supabase
-- This script is IDEMPOTENT — safe to run multiple times.
-- It dynamically finds the first orchard and creates a season + blocks + rows.
DO $$
DECLARE v_orchard_id UUID;
v_season_id UUID;
v_block_a_id UUID;
v_block_b_id UUID;
v_block_c_id UUID;
BEGIN -- 1. Find the first orchard (whatever it is)
SELECT id INTO v_orchard_id
FROM public.orchards
LIMIT 1;
IF v_orchard_id IS NULL THEN RAISE EXCEPTION 'No orchards found in database. Create an orchard first.';
END IF;
RAISE NOTICE 'Using orchard: %',
v_orchard_id;
-- 2. Find or create an active season
SELECT id INTO v_season_id
FROM public.harvest_seasons
WHERE orchard_id = v_orchard_id
    AND status = 'active'
    AND deleted_at IS NULL
LIMIT 1;
IF v_season_id IS NULL THEN
INSERT INTO public.harvest_seasons (orchard_id, name, start_date, status)
VALUES (
        v_orchard_id,
        'Season 2026',
        '2026-01-01',
        'active'
    )
RETURNING id INTO v_season_id;
RAISE NOTICE 'Created new season: %',
v_season_id;
ELSE RAISE NOTICE 'Using existing season: %',
v_season_id;
END IF;
-- 3. Delete old blocks/rows if re-running (soft delete)
UPDATE public.block_rows
SET deleted_at = now()
WHERE block_id IN (
        SELECT id
        FROM public.orchard_blocks
        WHERE season_id = v_season_id
            AND deleted_at IS NULL
    )
    AND deleted_at IS NULL;
UPDATE public.orchard_blocks
SET deleted_at = now()
WHERE season_id = v_season_id
    AND deleted_at IS NULL;
-- 4. Block A: Lapins (rows 1-10)
INSERT INTO public.orchard_blocks (
        orchard_id,
        season_id,
        name,
        total_rows,
        start_row,
        color_code,
        status
    )
VALUES (
        v_orchard_id,
        v_season_id,
        'Block A',
        10,
        1,
        '#dc2626',
        'active'
    )
RETURNING id INTO v_block_a_id;
INSERT INTO public.block_rows (block_id, row_number, variety, target_buckets)
SELECT v_block_a_id,
    r,
    'Lapins',
    100
FROM generate_series(1, 10) AS r;
-- 5. Block B: Sweetheart (rows 11-20)
INSERT INTO public.orchard_blocks (
        orchard_id,
        season_id,
        name,
        total_rows,
        start_row,
        color_code,
        status
    )
VALUES (
        v_orchard_id,
        v_season_id,
        'Block B',
        10,
        11,
        '#2563eb',
        'active'
    )
RETURNING id INTO v_block_b_id;
INSERT INTO public.block_rows (block_id, row_number, variety, target_buckets)
SELECT v_block_b_id,
    r,
    'Sweetheart',
    80
FROM generate_series(11, 20) AS r;
-- 6. Block C: Mixed (rows 21-30)
INSERT INTO public.orchard_blocks (
        orchard_id,
        season_id,
        name,
        total_rows,
        start_row,
        color_code,
        status
    )
VALUES (
        v_orchard_id,
        v_season_id,
        'Block C',
        10,
        21,
        '#16a34a',
        'idle'
    )
RETURNING id INTO v_block_c_id;
INSERT INTO public.block_rows (block_id, row_number, variety, target_buckets)
SELECT v_block_c_id,
    r,
    CASE
        WHEN r % 2 = 1 THEN 'Staccato'
        ELSE 'Skeena'
    END,
    120
FROM generate_series(21, 30) AS r;
RAISE NOTICE '✅ Seed complete: 3 blocks, 30 rows for orchard % season %',
v_orchard_id,
v_season_id;
END $$;