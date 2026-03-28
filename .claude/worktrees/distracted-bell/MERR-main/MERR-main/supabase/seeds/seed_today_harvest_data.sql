-- =============================================
-- SEED: Today's Harvest Data for J&P Cherries
-- Run AFTER seed_jp_cherries_blocks.sql
-- Run in Supabase SQL Editor (bypasses RLS)
-- IDEMPOTENT - safe to re-run
-- =============================================
-- FIX: trigger references NEW.timestamp but column is NEW.scanned_at
CREATE OR REPLACE FUNCTION public.enforce_closed_day_bucket_records() RETURNS TRIGGER LANGUAGE plpgsql SECURITY DEFINER
SET search_path = public AS $$
DECLARE v_closed_at TIMESTAMPTZ;
v_bucket_date DATE;
BEGIN v_bucket_date := DATE(NEW.scanned_at AT TIME ZONE 'Pacific/Auckland');
SELECT closed_at INTO v_closed_at
FROM day_closures
WHERE orchard_id = NEW.orchard_id
    AND date = v_bucket_date
    AND status = 'closed'
LIMIT 1;
IF v_closed_at IS NOT NULL THEN IF NEW.scanned_at >= v_closed_at THEN RAISE EXCEPTION 'INSERT_BLOCKED: Day % is closed for orchard %.',
v_bucket_date,
NEW.orchard_id USING ERRCODE = 'P0001';
END IF;
END IF;
RETURN NEW;
END;
$$;
-- Disable triggers for seed
ALTER TABLE public.bucket_records DISABLE TRIGGER trg_enforce_closed_day;
-- Clean previous seed data
DELETE FROM public.bucket_records
WHERE picker_id IN (
        SELECT id
        FROM public.pickers
        WHERE picker_id LIKE 'SIM-%'
    );
DELETE FROM public.daily_attendance
WHERE picker_id IN (
        SELECT id
        FROM public.pickers
        WHERE picker_id LIKE 'SIM-%'
    );
DELETE FROM public.pickers
WHERE picker_id LIKE 'SIM-%';
-- =============================================
-- 1. PICKERS (25)
-- =============================================
DO $$
DECLARE v_orchard_id UUID := '11111111-0001-0001-0001-000000000001';
v_names TEXT [] := ARRAY [
        'Aroha Walker', 'Tane Williams', 'Sofia Brown', 'Liam Smith',
        'Maia Taylor', 'Nikau Wilson', 'Jade Thompson', 'Kai White',
        'Emma Harris', 'Matiu Martin', 'Isla Anderson', 'Rawiri Thomas',
        'Olivia Jackson', 'Wiremu Clark', 'Anika Robinson', 'Hemi Lewis',
        'Lily Young', 'Rua Hall', 'Zoe King', 'Te Koha Wright',
        'Nina Baker', 'Ihaka Green', 'Mia Adams', 'Kahu Nelson', 'Eva Hill'
    ];
v_row_assignments INT [] := ARRAY [
        1, 1, 2, 3, 3,
        5, 6, 7, 8, 10,
        11, 11, 12, 14, 15,
        17, 18, 19, 20, 20,
        21, 22, 24, 26, 28
    ];
v_idx INT;
BEGIN FOR v_idx IN 1..25 LOOP
INSERT INTO public.pickers (
        id,
        picker_id,
        name,
        orchard_id,
        safety_verified,
        status,
        current_row
    )
VALUES (
        gen_random_uuid(),
        'SIM-' || LPAD(v_idx::TEXT, 3, '0'),
        v_names [v_idx],
        v_orchard_id,
        true,
        'active',
        v_row_assignments [v_idx]
    ) ON CONFLICT (picker_id) DO
UPDATE
SET current_row = EXCLUDED.current_row,
    status = EXCLUDED.status;
END LOOP;
RAISE NOTICE 'Inserted 25 simulation pickers';
END $$;
-- =============================================
-- 2. BUCKET RECORDS (~400) for TODAY
-- Now includes season_id from active season
-- =============================================
DO $$
DECLARE v_orchard_id UUID := '11111111-0001-0001-0001-000000000001';
v_season_id UUID;
v_picker RECORD;
v_bucket_count INT;
v_row_num INT;
v_hour INT;
v_minute INT;
v_scan_time TIMESTAMPTZ;
v_today DATE := (NOW() AT TIME ZONE 'Pacific/Auckland')::DATE;
v_total INT := 0;
BEGIN -- Get the active season for this orchard
SELECT id INTO v_season_id
FROM public.harvest_seasons
WHERE orchard_id = v_orchard_id
    AND status = 'active'
    AND deleted_at IS NULL
LIMIT 1;
IF v_season_id IS NULL THEN RAISE EXCEPTION 'No active season found for J&P Cherries. Run seed_jp_cherries_blocks.sql first.';
END IF;
RAISE NOTICE 'Using season_id: %',
v_season_id;
FOR v_picker IN
SELECT id,
    picker_id,
    current_row
FROM public.pickers
WHERE picker_id LIKE 'SIM-%'
    AND current_row > 0
ORDER BY picker_id LOOP v_row_num := v_picker.current_row;
CASE
    WHEN v_row_num IN (1, 2) THEN v_bucket_count := 45 + (random() * 15)::INT;
WHEN v_row_num IN (3, 11, 12) THEN v_bucket_count := 25 + (random() * 15)::INT;
WHEN v_row_num IN (5, 14, 15, 20) THEN v_bucket_count := 15 + (random() * 10)::INT;
WHEN v_row_num IN (6, 7, 8, 17, 18, 19) THEN v_bucket_count := 8 + (random() * 10)::INT;
WHEN v_row_num IN (10, 21, 22, 24, 26, 28) THEN v_bucket_count := 3 + (random() * 7)::INT;
ELSE v_bucket_count := 1 + (random() * 5)::INT;
END CASE
;
FOR i IN 1..v_bucket_count LOOP v_hour := 6 + ((i * 8) / v_bucket_count);
IF v_hour > 14 THEN v_hour := 14;
END IF;
v_minute := (random() * 59)::INT;
v_scan_time := (
    v_today::TEXT || ' ' || LPAD(v_hour::TEXT, 2, '0') || ':' || LPAD(v_minute::TEXT, 2, '0') || ':' || LPAD((random() * 59)::INT::TEXT, 2, '0') || '+13'
)::TIMESTAMPTZ;
INSERT INTO public.bucket_records (
        orchard_id,
        picker_id,
        season_id,
        scanned_at,
        row_number,
        coords,
        quality_grade
    )
VALUES (
        v_orchard_id,
        v_picker.id,
        v_season_id,
        v_scan_time,
        v_row_num,
        jsonb_build_object(
            'lat',
            -45.0318 + (v_row_num * 0.0003) + (random() * 0.0001),
            'lng',
            169.1870 + (v_row_num * 0.0002) + (random() * 0.0001)
        ),
        CASE
            WHEN random() < 0.50 THEN 'A'
            WHEN random() < 0.80 THEN 'B'
            WHEN random() < 0.95 THEN 'C'
            ELSE 'reject'
        END
    );
v_total := v_total + 1;
END LOOP;
END LOOP;
RAISE NOTICE 'Inserted % bucket records for today (%)',
v_total,
v_today;
END $$;
-- Re-enable the trigger
ALTER TABLE public.bucket_records ENABLE TRIGGER trg_enforce_closed_day;
-- =============================================
-- 3. DAILY ATTENDANCE for today
-- =============================================
DO $$
DECLARE v_orchard_id UUID := '11111111-0001-0001-0001-000000000001';
v_today DATE := (NOW() AT TIME ZONE 'Pacific/Auckland')::DATE;
v_picker RECORD;
v_checkin TIMESTAMPTZ;
BEGIN FOR v_picker IN
SELECT id
FROM public.pickers
WHERE picker_id LIKE 'SIM-%'
    AND status = 'active' LOOP v_checkin := (
        v_today::TEXT || ' 06:' || LPAD((20 + (random() * 20)::INT)::TEXT, 2, '0') || ':00+13'
    )::TIMESTAMPTZ;
INSERT INTO public.daily_attendance (
        picker_id,
        orchard_id,
        date,
        status,
        check_in_time,
        hours_worked
    )
VALUES (
        v_picker.id,
        v_orchard_id,
        v_today,
        'present',
        v_checkin,
        6 + (random() * 2)::DECIMAL
    ) ON CONFLICT DO NOTHING;
END LOOP;
RAISE NOTICE 'Inserted daily attendance for today';
END $$;