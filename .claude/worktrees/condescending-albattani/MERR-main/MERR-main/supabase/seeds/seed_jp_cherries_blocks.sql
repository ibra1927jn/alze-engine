-- FIX: Create season + blocks for J&P Cherries (the ACTIVE orchard)
-- The previous seed created data for the wrong orchard.
-- Run this in Supabase SQL Editor.
DO $$
DECLARE v_orchard_id UUID := '11111111-0001-0001-0001-000000000001';
-- J&P Cherries
v_season_id UUID;
v_block_a_id UUID;
v_block_b_id UUID;
v_block_c_id UUID;
BEGIN -- 1. Create active season for J&P Cherries
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
RAISE NOTICE 'Created season: %',
v_season_id;
ELSE RAISE NOTICE 'Season already exists: %',
v_season_id;
END IF;
-- 2. Block A: Lapins (rows 1-10, red)
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
-- 3. Block B: Sweetheart (rows 11-20, blue)
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
-- 4. Block C: Mixed (rows 21-30, green)
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
RAISE NOTICE '✅ Done: 3 blocks, 30 rows for J&P Cherries season %',
v_season_id;
END $$;