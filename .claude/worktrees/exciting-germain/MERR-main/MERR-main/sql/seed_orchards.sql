-- seed_orchards.sql
-- Populate orchards table with REAL Central Pac sector names

-- 1. Ensure the default orchard exists
INSERT INTO public.orchards (id, name, location, total_rows, hectares)
VALUES 
    ('a0000000-0000-0000-0000-000000000001', 'SMILING TIGER', 'Hastings, Hawkes Bay', 480, 12.5)
ON CONFLICT (id) DO NOTHING;

-- 2. Add more orchards from Central Pac with REAL names
INSERT INTO public.orchards (id, name, location, total_rows, hectares) VALUES
    (gen_random_uuid(), 'COOPER LANE', 'Hastings, Hawkes Bay', 520, 14.2),
    (gen_random_uuid(), 'GOLDEN HARVEST', 'Gisborne', 320, 8.0),
    (gen_random_uuid(), 'SUNRISE BLOCK', 'Havelock North', 550, 15.0),
    (gen_random_uuid(), 'PACIFIC VIEW', 'Meeanee', 280, 7.2),
    (gen_random_uuid(), 'BLUE MOUNTAIN', 'Hastings', 400, 10.5),
    (gen_random_uuid(), 'RED RIVER', 'Hastings', 650, 18.0),
    (gen_random_uuid(), 'JADE VALLEY', 'Hastings', 220, 5.8),
    (gen_random_uuid(), 'SILVER FERN', 'Napier', 380, 9.5),
    (gen_random_uuid(), 'EMERALD GROVE', 'Hastings', 420, 11.0),
    (gen_random_uuid(), 'KIWI GOLD', 'Hastings', 300, 7.8),
    (gen_random_uuid(), 'MOONLIGHT BAY', 'Gisborne', 340, 9.0),
    (gen_random_uuid(), 'THUNDERBIRD', 'Napier', 460, 12.0)
ON CONFLICT DO NOTHING;

-- 3. Verify
SELECT id, name, location, total_rows FROM public.orchards ORDER BY name;
