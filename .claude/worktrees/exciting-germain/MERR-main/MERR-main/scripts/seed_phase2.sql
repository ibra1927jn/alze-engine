-- =============================================
-- SEED: Phase 2 Demo Data
-- Prerequisites: 20260213_phase2_tables.sql applied
-- Run this AFTER the migration in Supabase SQL Editor
--
-- Data:
--   5 contracts (2 permanent, 2 seasonal, 1 casual)
--   3 fleet vehicles (1 active, 1 idle, 1 maintenance)
--   10 transport requests (mixed statuses)
-- =============================================
-- Get the default orchard ID
DO $$
DECLARE v_orchard_id UUID;
v_manager_id UUID;
v_lead_id UUID;
v_logistics_id UUID;
v_hr_id UUID;
v_runner_id UUID;
v_vehicle_1 UUID := gen_random_uuid();
v_vehicle_2 UUID := gen_random_uuid();
v_vehicle_3 UUID := gen_random_uuid();
BEGIN -- Get orchard
SELECT id INTO v_orchard_id
FROM public.orchards
LIMIT 1;
IF v_orchard_id IS NULL THEN RAISE EXCEPTION 'No orchard found. Run schema_v1_consolidated.sql first.';
END IF;
-- Get demo user IDs
SELECT u.id INTO v_manager_id
FROM public.users u
WHERE u.role = 'manager'
LIMIT 1;
SELECT u.id INTO v_lead_id
FROM public.users u
WHERE u.role = 'team_leader'
LIMIT 1;
SELECT u.id INTO v_logistics_id
FROM public.users u
WHERE u.role = 'logistics'
LIMIT 1;
SELECT u.id INTO v_hr_id
FROM public.users u
WHERE u.role = 'hr_admin'
LIMIT 1;
SELECT u.id INTO v_runner_id
FROM public.users u
WHERE u.role = 'runner'
LIMIT 1;
-- =============================================
-- CONTRACTS (5 entries)
-- =============================================
-- 1. Permanent - Manager (active, no end date)
INSERT INTO public.contracts (
        employee_id,
        orchard_id,
        type,
        status,
        start_date,
        end_date,
        hourly_rate,
        notes,
        created_by
    )
SELECT v_manager_id,
    v_orchard_id,
    'permanent',
    'active',
    '2025-06-01',
    NULL,
    35.00,
    'Orchard Manager - full time permanent contract',
    v_hr_id
WHERE v_manager_id IS NOT NULL
    AND NOT EXISTS (
        SELECT 1
        FROM public.contracts
        WHERE employee_id = v_manager_id
            AND status = 'active'
    );
-- 2. Permanent - Logistics Coordinator (active)
INSERT INTO public.contracts (
        employee_id,
        orchard_id,
        type,
        status,
        start_date,
        end_date,
        hourly_rate,
        notes,
        created_by
    )
SELECT v_logistics_id,
    v_orchard_id,
    'permanent',
    'active',
    '2025-08-15',
    NULL,
    28.50,
    'Logistics Coordinator - permanent position',
    v_hr_id
WHERE v_logistics_id IS NOT NULL
    AND NOT EXISTS (
        SELECT 1
        FROM public.contracts
        WHERE employee_id = v_logistics_id
            AND status = 'active'
    );
-- 3. Seasonal - Team Leader (expiring soon - 14 days)
INSERT INTO public.contracts (
        employee_id,
        orchard_id,
        type,
        status,
        start_date,
        end_date,
        hourly_rate,
        notes,
        created_by
    )
SELECT v_lead_id,
    v_orchard_id,
    'seasonal',
    'expiring',
    '2025-11-01',
    (CURRENT_DATE + INTERVAL '14 days')::DATE,
    26.00,
    'Seasonal team leader - kiwifruit harvest season. Renewal pending.',
    v_hr_id
WHERE v_lead_id IS NOT NULL
    AND NOT EXISTS (
        SELECT 1
        FROM public.contracts
        WHERE employee_id = v_lead_id
            AND status IN ('active', 'expiring')
    );
-- 4. Seasonal - Runner (active, ends March)
INSERT INTO public.contracts (
        employee_id,
        orchard_id,
        type,
        status,
        start_date,
        end_date,
        hourly_rate,
        notes,
        created_by
    )
SELECT v_runner_id,
    v_orchard_id,
    'seasonal',
    'active',
    '2025-11-15',
    '2026-03-31',
    23.50,
    'Bucket runner - harvest season. RSE work visa holder.',
    v_hr_id
WHERE v_runner_id IS NOT NULL
    AND NOT EXISTS (
        SELECT 1
        FROM public.contracts
        WHERE employee_id = v_runner_id
            AND status = 'active'
    );
-- 5. Casual - HR Admin (draft, not yet signed)
INSERT INTO public.contracts (
        employee_id,
        orchard_id,
        type,
        status,
        start_date,
        end_date,
        hourly_rate,
        notes,
        created_by
    )
SELECT v_hr_id,
    v_orchard_id,
    'casual',
    'draft',
    CURRENT_DATE,
    (CURRENT_DATE + INTERVAL '90 days')::DATE,
    25.00,
    'HR Administrator - casual/on-call basis during peak season',
    v_manager_id
WHERE v_hr_id IS NOT NULL
    AND NOT EXISTS (
        SELECT 1
        FROM public.contracts
        WHERE employee_id = v_hr_id
            AND status = 'draft'
    );
-- =============================================
-- FLEET VEHICLES (3 entries)
-- =============================================
INSERT INTO public.fleet_vehicles (
        id,
        orchard_id,
        name,
        registration,
        zone,
        driver_id,
        driver_name,
        status,
        load_status,
        bins_loaded,
        max_capacity,
        fuel_level,
        last_service_date,
        next_service_date,
        wof_expiry
    )
VALUES -- T-001: Active, partial load, in zone A2
    (
        v_vehicle_1,
        v_orchard_id,
        'T-001',
        'KHV 421',
        'A2',
        v_logistics_id,
        'Logistics User',
        'active',
        'partial',
        3,
        8,
        72,
        '2026-01-10',
        '2026-04-10',
        '2026-08-15'
    ),
    -- T-002: Idle, empty, parked
    (
        v_vehicle_2,
        v_orchard_id,
        'T-002',
        'BQR 883',
        NULL,
        NULL,
        NULL,
        'idle',
        'empty',
        0,
        8,
        45,
        '2025-12-20',
        '2026-03-20',
        '2026-06-01'
    ),
    -- T-003: Maintenance, low fuel
    (
        v_vehicle_3,
        v_orchard_id,
        'T-003',
        'FNP 156',
        'B1',
        NULL,
        NULL,
        'maintenance',
        'empty',
        0,
        10,
        15,
        '2026-02-01',
        '2026-02-28',
        '2026-09-30'
    ) ON CONFLICT (id) DO NOTHING;
-- =============================================
-- TRANSPORT REQUESTS (10 entries)
-- =============================================
-- Use requester IDs only if they exist
IF v_lead_id IS NOT NULL
AND v_logistics_id IS NOT NULL THEN
INSERT INTO public.transport_requests (
        orchard_id,
        requested_by,
        requester_name,
        zone,
        bins_count,
        priority,
        status,
        assigned_vehicle,
        assigned_by,
        notes,
        completed_at,
        created_at
    )
VALUES -- 1. URGENT - pending, just submitted
    (
        v_orchard_id,
        v_lead_id,
        'Team Leader',
        'A2',
        6,
        'urgent',
        'pending',
        NULL,
        NULL,
        'Full bins blocking row 12-14. Need pickup ASAP.',
        NULL,
        now() - INTERVAL '5 minutes'
    ),
    -- 2. HIGH - assigned to T-001, in progress
    (
        v_orchard_id,
        v_lead_id,
        'Team Leader',
        'A3',
        4,
        'high',
        'in_progress',
        v_vehicle_1,
        v_logistics_id,
        'Bins nearly overflowing. T-001 dispatched.',
        NULL,
        now() - INTERVAL '25 minutes'
    ),
    -- 3. NORMAL - assigned, waiting
    (
        v_orchard_id,
        v_runner_id,
        'Runner',
        'B1',
        2,
        'normal',
        'assigned',
        v_vehicle_2,
        v_logistics_id,
        'Routine pickup.',
        NULL,
        now() - INTERVAL '45 minutes'
    ),
    -- 4. COMPLETED - done 1 hour ago
    (
        v_orchard_id,
        v_lead_id,
        'Team Leader',
        'A1',
        5,
        'high',
        'completed',
        v_vehicle_1,
        v_logistics_id,
        'Morning pickup completed.',
        now() - INTERVAL '30 minutes',
        now() - INTERVAL '2 hours'
    ),
    -- 5. COMPLETED - done 2 hours ago
    (
        v_orchard_id,
        v_runner_id,
        'Runner',
        'C1',
        3,
        'normal',
        'completed',
        v_vehicle_2,
        v_logistics_id,
        'Delivered to warehouse shed 2.',
        now() - INTERVAL '1 hour 30 minutes',
        now() - INTERVAL '3 hours'
    ),
    -- 6. COMPLETED - early morning
    (
        v_orchard_id,
        v_lead_id,
        'Team Leader',
        'A2',
        8,
        'urgent',
        'completed',
        v_vehicle_1,
        v_logistics_id,
        'First pickup of the day - all zones.',
        now() - INTERVAL '4 hours',
        now() - INTERVAL '5 hours'
    ),
    -- 7. PENDING - normal priority
    (
        v_orchard_id,
        v_lead_id,
        'Team Leader',
        'B2',
        3,
        'normal',
        'pending',
        NULL,
        NULL,
        'Bins filling up in block B2.',
        NULL,
        now() - INTERVAL '10 minutes'
    ),
    -- 8. CANCELLED - no longer needed
    (
        v_orchard_id,
        v_runner_id,
        'Runner',
        'C1',
        1,
        'normal',
        'cancelled',
        NULL,
        NULL,
        'Picker moved to different row. Not needed.',
        NULL,
        now() - INTERVAL '1 hour'
    ),
    -- 9. COMPLETED - yesterday
    (
        v_orchard_id,
        v_lead_id,
        'Team Leader',
        'A1',
        7,
        'high',
        'completed',
        v_vehicle_1,
        v_logistics_id,
        'End-of-day full collection.',
        now() - INTERVAL '20 hours',
        now() - INTERVAL '22 hours'
    ),
    -- 10. PENDING - high priority
    (
        v_orchard_id,
        v_lead_id,
        'Team Leader',
        'A3',
        5,
        'high',
        'pending',
        NULL,
        NULL,
        'Grade A bins — need cold storage ASAP.',
        NULL,
        now() - INTERVAL '2 minutes'
    );
END IF;
RAISE NOTICE 'Phase 2 seed data inserted successfully for orchard %',
v_orchard_id;
END $$;
-- =============================================
-- VERIFICATION QUERIES
-- =============================================
SELECT 'Contracts' as table_name,
    count(*) as row_count
FROM public.contracts
UNION ALL
SELECT 'Fleet Vehicles',
    count(*)
FROM public.fleet_vehicles
UNION ALL
SELECT 'Transport Requests',
    count(*)
FROM public.transport_requests;
-- Show contract details
SELECT c.type,
    c.status,
    c.start_date,
    c.end_date,
    c.hourly_rate,
    u.full_name
FROM public.contracts c
    LEFT JOIN public.users u ON c.employee_id = u.id
ORDER BY c.status,
    c.type;
-- Show fleet
SELECT name,
    registration,
    zone,
    driver_name,
    status,
    fuel_level,
    wof_expiry
FROM public.fleet_vehicles
ORDER BY name;
-- Show active transport requests
SELECT zone,
    bins_count,
    priority,
    status,
    requester_name,
    ROUND(
        EXTRACT(
            EPOCH
            FROM (now() - created_at)
        ) / 60
    ) as minutes_ago
FROM public.transport_requests
WHERE status IN ('pending', 'assigned', 'in_progress')
ORDER BY CASE
        priority
        WHEN 'urgent' THEN 1
        WHEN 'high' THEN 2
        ELSE 3
    END,
    created_at;