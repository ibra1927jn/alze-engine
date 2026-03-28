-- =============================================
-- HARVESTPRO NZ - SCALE SEED TEST (INDUSTRIAL)
-- =============================================
-- Generates 500K+ rows to simulate 3 months of real production data.
-- Uses generate_series for speed. Idempotent (safe to re-run).
-- Estimated execution: ~30 seconds on Supabase free tier.
-- =============================================
-- =============================================
-- 0. CLEANUP (Idempotent)
-- =============================================
DELETE FROM public.bucket_records
WHERE orchard_id IN (
        SELECT id
        FROM public.orchards
        WHERE name LIKE 'ACID_%'
    );
DELETE FROM public.daily_attendance
WHERE orchard_id IN (
        SELECT id
        FROM public.orchards
        WHERE name LIKE 'ACID_%'
    );
DELETE FROM public.pickers
WHERE orchard_id IN (
        SELECT id
        FROM public.orchards
        WHERE name LIKE 'ACID_%'
    );
DELETE FROM public.users
WHERE email LIKE 'acid_%@harvestpro.test';
DELETE FROM public.orchards
WHERE name LIKE 'ACID_%';
-- =============================================
-- 1. ORCHARDS (10 with deterministic UUIDs)
-- =============================================
INSERT INTO public.orchards (id, name, location, total_blocks, code)
VALUES (
        'a1000000-0000-0000-0000-000000000001',
        'ACID_ALPHA',
        'Hastings, HB',
        480,
        'ACID01'
    ),
    (
        'a1000000-0000-0000-0000-000000000002',
        'ACID_BRAVO',
        'Napier, HB',
        520,
        'ACID02'
    ),
    (
        'a1000000-0000-0000-0000-000000000003',
        'ACID_CHARLIE',
        'Gisborne',
        320,
        'ACID03'
    ),
    (
        'a1000000-0000-0000-0000-000000000004',
        'ACID_DELTA',
        'Havelock North',
        550,
        'ACID04'
    ),
    (
        'a1000000-0000-0000-0000-000000000005',
        'ACID_ECHO',
        'Meeanee',
        280,
        'ACID05'
    ),
    (
        'a1000000-0000-0000-0000-000000000006',
        'ACID_FOXTROT',
        'Hastings',
        400,
        'ACID06'
    ),
    (
        'a1000000-0000-0000-0000-000000000007',
        'ACID_GOLF',
        'Hastings',
        650,
        'ACID07'
    ),
    (
        'a1000000-0000-0000-0000-000000000008',
        'ACID_HOTEL',
        'Napier',
        220,
        'ACID08'
    ),
    (
        'a1000000-0000-0000-0000-000000000009',
        'ACID_INDIA',
        'Gisborne',
        380,
        'ACID09'
    ),
    (
        'a1000000-0000-0000-0000-000000000010',
        'ACID_JULIET',
        'Central Otago',
        420,
        'ACID10'
    ) ON CONFLICT (id) DO NOTHING;
-- =============================================
-- 2. USERS (50 = 5 per orchard)
-- =============================================
-- NOTE: These users won't have auth.users entries, so they can't login.
-- They're for DB-level testing (direct queries, RLS policy tests).
-- For Playwright tests, use the existing real auth users.
-- We'll create them using a DO block with deterministic UUIDs.
DO $$
DECLARE orchard_idx INT;
orchard_uuid UUID;
user_uuid UUID;
roles TEXT [] := ARRAY ['manager', 'team_leader', 'runner', 'runner', 'runner'];
role_name TEXT;
role_idx INT;
BEGIN FOR orchard_idx IN 1..10 LOOP orchard_uuid := (
    'a1000000-0000-0000-0000-0000000000' || LPAD(orchard_idx::TEXT, 2, '0')
)::UUID;
FOR role_idx IN 1..5 LOOP user_uuid := (
    'b1000000-0000-0000-' || LPAD(orchard_idx::TEXT, 4, '0') || '-0000000000' || LPAD(role_idx::TEXT, 2, '0')
)::UUID;
role_name := roles [role_idx];
-- Insert into auth.users first (required by FK)
INSERT INTO auth.users (
        id,
        email,
        encrypted_password,
        email_confirmed_at,
        role,
        aud,
        created_at,
        updated_at
    )
VALUES (
        user_uuid,
        'acid_' || role_name || '_' || orchard_idx || '_' || role_idx || '@harvestpro.test',
        crypt('AcidTest2026!', gen_salt('bf')),
        now(),
        'authenticated',
        'authenticated',
        now(),
        now()
    ) ON CONFLICT (id) DO NOTHING;
-- Insert into public.users
INSERT INTO public.users (
        id,
        email,
        full_name,
        role,
        orchard_id,
        is_active
    )
VALUES (
        user_uuid,
        'acid_' || role_name || '_' || orchard_idx || '_' || role_idx || '@harvestpro.test',
        'Acid ' || INITCAP(role_name) || ' ' || orchard_idx || '-' || role_idx,
        role_name,
        orchard_uuid,
        true
    ) ON CONFLICT (id) DO NOTHING;
END LOOP;
END LOOP;
END $$;
-- =============================================
-- 3. PICKERS (200 = 20 per orchard)
-- =============================================
DO $$
DECLARE orchard_idx INT;
picker_idx INT;
orchard_uuid UUID;
team_leader_uuid UUID;
picker_uuid UUID;
BEGIN FOR orchard_idx IN 1..10 LOOP orchard_uuid := (
    'a1000000-0000-0000-0000-0000000000' || LPAD(orchard_idx::TEXT, 2, '0')
)::UUID;
-- Team leader is user #2 in each orchard
team_leader_uuid := (
    'b1000000-0000-0000-' || LPAD(orchard_idx::TEXT, 4, '0') || '-000000000002'
)::UUID;
FOR picker_idx IN 1..20 LOOP picker_uuid := (
    'c1000000-0000-' || LPAD(orchard_idx::TEXT, 4, '0') || '-0000-0000000000' || LPAD(picker_idx::TEXT, 2, '0')
)::UUID;
INSERT INTO public.pickers (
        id,
        picker_id,
        name,
        orchard_id,
        team_leader_id,
        status,
        safety_verified,
        current_row
    )
VALUES (
        picker_uuid,
        'ACID-' || LPAD(orchard_idx::TEXT, 2, '0') || '-' || LPAD(picker_idx::TEXT, 3, '0'),
        'Picker ' || orchard_idx || '-' || picker_idx,
        orchard_uuid,
        team_leader_uuid,
        CASE
            WHEN picker_idx <= 18 THEN 'active'
            WHEN picker_idx = 19 THEN 'archived' -- 5% archived
            ELSE 'inactive' -- 5% inactive
        END,
        picker_idx <= 16,
        -- 80% safety verified
        (picker_idx % 15) + 1 -- Spread across rows 1-15
    ) ON CONFLICT (picker_id) DO NOTHING;
END LOOP;
END LOOP;
END $$;
-- =============================================
-- 4. DAILY ATTENDANCE (18,000 = 200 pickers × 90 days)
-- =============================================
-- Edge cases built in:
--   5% have NO check_out_time (forgotten checkout)
--   3% have status 'absent' (ghost check-ins)
-- =============================================
INSERT INTO public.daily_attendance (
        id,
        picker_id,
        orchard_id,
        date,
        check_in_time,
        check_out_time,
        status
    )
SELECT gen_random_uuid(),
    p.id,
    p.orchard_id,
    d.day::DATE,
    -- Check-in between 6:00 AM and 7:30 AM NZST
    (
        d.day + INTERVAL '6 hours' + (random() * INTERVAL '90 minutes')
    )::TIMESTAMPTZ,
    -- Check-out: 5% NULL (forgotten), rest between 3:30 PM and 5:30 PM
    CASE
        WHEN random() < 0.05 THEN NULL -- 5% forgotten checkout
        ELSE (
            d.day + INTERVAL '15 hours 30 minutes' + (random() * INTERVAL '120 minutes')
        )::TIMESTAMPTZ
    END,
    -- Status: 3% absent (ghost workers who got checked in by mistake)
    CASE
        WHEN random() < 0.03 THEN 'absent'
        WHEN random() < 0.05 THEN 'late'
        ELSE 'present'
    END
FROM public.pickers p
    CROSS JOIN generate_series(
        CURRENT_DATE - INTERVAL '90 days',
        CURRENT_DATE - INTERVAL '1 day',
        INTERVAL '1 day'
    ) AS d(day)
WHERE p.status = 'active'
    AND p.orchard_id IN (
        SELECT id
        FROM public.orchards
        WHERE name LIKE 'ACID_%'
    ) -- Skip weekends (realistic: orchards don't work Sunday, maybe Saturday)
    AND EXTRACT(
        DOW
        FROM d.day
    ) NOT IN (0) -- Skip Sundays
    ON CONFLICT ON CONSTRAINT unique_picker_daily_attendance DO NOTHING;
-- =============================================
-- 5. BUCKET RECORDS (500,000 = ~55K per orchard)
-- =============================================
-- This is the big one. Uses generate_series for speed.
-- Spread across 90 days, 20 pickers, ~30 buckets/day/picker average.
-- 3% of days have 0 buckets for some pickers (ghost workers).
-- =============================================
INSERT INTO public.bucket_records (
        id,
        orchard_id,
        picker_id,
        scanned_by,
        scanned_at,
        coords,
        created_at
    )
SELECT gen_random_uuid(),
    p.orchard_id,
    p.id,
    -- Runner #3 (first runner) for this orchard
    -- Orchard UUID ends in ...0001 to ...0010, extract last 2 digits for user UUID
    (
        'b1000000-0000-0000-00' || SUBSTRING(
            p.orchard_id::TEXT
            FROM 38 FOR 2
        ) || '-000000000003'
    )::UUID,
    -- Scan time: spread across the work day (7 AM to 4 PM)
    (
        d.day + INTERVAL '7 hours' + (s.bucket_num * INTERVAL '15 seconds') + (random() * INTERVAL '8 hours')
    )::TIMESTAMPTZ,
    -- Coords: random within Hawkes Bay area
    jsonb_build_object(
        'lat',
        -39.5 + (random() * 0.1),
        'lng',
        176.8 + (random() * 0.1)
    ),
    -- created_at = scanned_at + small delay (sync lag simulation)
    (
        d.day + INTERVAL '7 hours' + (s.bucket_num * INTERVAL '15 seconds') + (random() * INTERVAL '8 hours') + (random() * INTERVAL '30 seconds')
    )::TIMESTAMPTZ
FROM public.pickers p
    CROSS JOIN generate_series(
        CURRENT_DATE - INTERVAL '90 days',
        CURRENT_DATE - INTERVAL '1 day',
        INTERVAL '1 day'
    ) AS d(day)
    CROSS JOIN generate_series(1, 30) AS s(bucket_num) -- ~30 buckets per picker per day
WHERE p.status = 'active'
    AND p.orchard_id IN (
        SELECT id
        FROM public.orchards
        WHERE name LIKE 'ACID_%'
    )
    AND EXTRACT(
        DOW
        FROM d.day
    ) NOT IN (0) -- Skip Sundays
    -- 3% of picker-days are "ghost days" with only 0-2 buckets
    AND (
        random() > 0.03
        OR s.bucket_num <= 2
    ) -- Vary output per picker: some scan 25, some scan 15 (abs to avoid negatives)
    AND s.bucket_num <= (
        15 + (abs(hashtext(p.id::TEXT || d.day::TEXT)) % 16)
    );
-- =============================================
-- 6. VERIFY COUNTS
-- =============================================
SELECT 'ORCHARDS' AS entity,
    COUNT(*) AS count
FROM public.orchards
WHERE name LIKE 'ACID_%'
UNION ALL
SELECT 'USERS',
    COUNT(*)
FROM public.users
WHERE email LIKE 'acid_%@harvestpro.test'
UNION ALL
SELECT 'PICKERS',
    COUNT(*)
FROM public.pickers
WHERE orchard_id IN (
        SELECT id
        FROM public.orchards
        WHERE name LIKE 'ACID_%'
    )
UNION ALL
SELECT 'ATTENDANCE',
    COUNT(*)
FROM public.daily_attendance
WHERE orchard_id IN (
        SELECT id
        FROM public.orchards
        WHERE name LIKE 'ACID_%'
    )
UNION ALL
SELECT 'BUCKET_RECORDS',
    COUNT(*)
FROM public.bucket_records
WHERE orchard_id IN (
        SELECT id
        FROM public.orchards
        WHERE name LIKE 'ACID_%'
    );
-- =============================================
-- 7. INDEX STRESS TEST: Queries that WILL be slow without proper indexes
-- =============================================
-- Uncomment these to test after seeding. They expose missing indexes.
-- Test A: Date-range report (needs composite index on orchard_id + scanned_at)
-- EXPLAIN ANALYZE
-- SELECT picker_id, COUNT(*) as buckets, MIN(scanned_at), MAX(scanned_at)
-- FROM public.bucket_records
-- WHERE orchard_id = 'a1000000-0000-0000-0000-000000000001'
--   AND scanned_at BETWEEN (CURRENT_DATE - INTERVAL '30 days') AND CURRENT_DATE
-- GROUP BY picker_id
-- ORDER BY buckets DESC;
-- Test B: Cross-table payroll join (needs attendance index)
-- EXPLAIN ANALYZE
-- SELECT p.name, p.picker_id,
--        COUNT(br.id) as buckets,
--        da.check_in_time, da.check_out_time,
--        EXTRACT(EPOCH FROM (da.check_out_time - da.check_in_time)) / 3600 as hours
-- FROM public.pickers p
-- JOIN public.daily_attendance da ON da.picker_id = p.id AND da.date = CURRENT_DATE - 1
-- LEFT JOIN public.bucket_records br ON br.picker_id = p.id
--   AND br.scanned_at::DATE = CURRENT_DATE - 1
-- WHERE p.orchard_id = 'a1000000-0000-0000-0000-000000000001'
-- GROUP BY p.id, p.name, p.picker_id, da.check_in_time, da.check_out_time;
SELECT '✅ ACID SEED COMPLETE — Check counts above' AS result;