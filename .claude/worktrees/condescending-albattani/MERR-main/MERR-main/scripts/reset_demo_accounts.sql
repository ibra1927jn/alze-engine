-- =============================================
-- RESET & CREATE DEMO ACCOUNTS
-- Deletes ALL existing users, creates 7 clean accounts
-- Password for ALL: 111111
-- ⚠️ Run in Supabase SQL Editor with service_role
-- =============================================
-- STEP 1: Clean all dependent tables first (FK constraints)
DELETE FROM public.audit_logs;
DELETE FROM public.daily_attendance;
DELETE FROM public.quality_inspections;
DELETE FROM public.qc_inspections;
DELETE FROM public.bucket_records;
DELETE FROM public.transport_requests;
DELETE FROM public.fleet_vehicles;
DELETE FROM public.contracts;
DELETE FROM public.sync_conflicts;
DELETE FROM public.chat_messages;
DELETE FROM public.conversations;
DELETE FROM public.day_setups;
DELETE FROM public.day_closures;
DELETE FROM public.pickers;
DELETE FROM public.bins;
SELECT '✅ Dependent tables cleaned' AS step;
-- STEP 1b: Clean public.users
DELETE FROM public.users;
-- STEP 2: Clean auth.identities (FK constraint)
DELETE FROM auth.identities;
-- STEP 3: Clean auth.users
DELETE FROM auth.users;
SELECT '✅ All old users deleted' AS step;
-- STEP 4: Create 7 demo accounts in auth.users
-- Password: 111111 (bcrypt hashed)
INSERT INTO auth.users (
        id,
        instance_id,
        email,
        encrypted_password,
        email_confirmed_at,
        created_at,
        updated_at,
        raw_app_meta_data,
        raw_user_meta_data,
        aud,
        role,
        confirmation_token
    )
VALUES -- 1. Manager
    (
        'a0000000-0000-0000-0001-000000000001',
        '00000000-0000-0000-0000-000000000000',
        'manager@harvestpro.nz',
        crypt('111111', gen_salt('bf')),
        now(),
        now(),
        now(),
        '{"provider":"email","providers":["email"]}',
        '{"full_name":"James Wilson"}',
        'authenticated',
        'authenticated',
        ''
    ),
    -- 2. Team Leader
    (
        'a0000000-0000-0000-0001-000000000002',
        '00000000-0000-0000-0000-000000000000',
        'lead@harvestpro.nz',
        crypt('111111', gen_salt('bf')),
        now(),
        now(),
        now(),
        '{"provider":"email","providers":["email"]}',
        '{"full_name":"Sarah Thompson"}',
        'authenticated',
        'authenticated',
        ''
    ),
    -- 3. Runner
    (
        'a0000000-0000-0000-0001-000000000003',
        '00000000-0000-0000-0000-000000000000',
        'runner@harvestpro.nz',
        crypt('111111', gen_salt('bf')),
        now(),
        now(),
        now(),
        '{"provider":"email","providers":["email"]}',
        '{"full_name":"Liam Ngata"}',
        'authenticated',
        'authenticated',
        ''
    ),
    -- 4. HR Admin
    (
        'a0000000-0000-0000-0001-000000000004',
        '00000000-0000-0000-0000-000000000000',
        'hr@harvestpro.nz',
        crypt('111111', gen_salt('bf')),
        now(),
        now(),
        now(),
        '{"provider":"email","providers":["email"]}',
        '{"full_name":"Maria González"}',
        'authenticated',
        'authenticated',
        ''
    ),
    -- 5. Logistics
    (
        'a0000000-0000-0000-0001-000000000005',
        '00000000-0000-0000-0000-000000000000',
        'logistics@harvestpro.nz',
        crypt('111111', gen_salt('bf')),
        now(),
        now(),
        now(),
        '{"provider":"email","providers":["email"]}',
        '{"full_name":"Carlos Muñoz"}',
        'authenticated',
        'authenticated',
        ''
    ),
    -- 6. Payroll Admin
    (
        'a0000000-0000-0000-0001-000000000006',
        '00000000-0000-0000-0000-000000000000',
        'payroll@harvestpro.nz',
        crypt('111111', gen_salt('bf')),
        now(),
        now(),
        now(),
        '{"provider":"email","providers":["email"]}',
        '{"full_name":"Ana Torres"}',
        'authenticated',
        'authenticated',
        ''
    ),
    -- 7. Admin
    (
        'a0000000-0000-0000-0001-000000000007',
        '00000000-0000-0000-0000-000000000000',
        'admin@harvestpro.nz',
        crypt('111111', gen_salt('bf')),
        now(),
        now(),
        now(),
        '{"provider":"email","providers":["email"]}',
        '{"full_name":"David Chen"}',
        'authenticated',
        'authenticated',
        ''
    );
SELECT '✅ 7 auth accounts created' AS step;
-- STEP 5: Create auth.identities (required by Supabase Auth)
INSERT INTO auth.identities (
        id,
        user_id,
        provider_id,
        provider,
        identity_data,
        last_sign_in_at,
        created_at,
        updated_at
    )
VALUES (
        'a0000000-0000-0000-0001-000000000001',
        'a0000000-0000-0000-0001-000000000001',
        'manager@harvestpro.nz',
        'email',
        jsonb_build_object(
            'sub',
            'a0000000-0000-0000-0001-000000000001',
            'email',
            'manager@harvestpro.nz'
        ),
        now(),
        now(),
        now()
    ),
    (
        'a0000000-0000-0000-0001-000000000002',
        'a0000000-0000-0000-0001-000000000002',
        'lead@harvestpro.nz',
        'email',
        jsonb_build_object(
            'sub',
            'a0000000-0000-0000-0001-000000000002',
            'email',
            'lead@harvestpro.nz'
        ),
        now(),
        now(),
        now()
    ),
    (
        'a0000000-0000-0000-0001-000000000003',
        'a0000000-0000-0000-0001-000000000003',
        'runner@harvestpro.nz',
        'email',
        jsonb_build_object(
            'sub',
            'a0000000-0000-0000-0001-000000000003',
            'email',
            'runner@harvestpro.nz'
        ),
        now(),
        now(),
        now()
    ),
    (
        'a0000000-0000-0000-0001-000000000004',
        'a0000000-0000-0000-0001-000000000004',
        'hr@harvestpro.nz',
        'email',
        jsonb_build_object(
            'sub',
            'a0000000-0000-0000-0001-000000000004',
            'email',
            'hr@harvestpro.nz'
        ),
        now(),
        now(),
        now()
    ),
    (
        'a0000000-0000-0000-0001-000000000005',
        'a0000000-0000-0000-0001-000000000005',
        'logistics@harvestpro.nz',
        'email',
        jsonb_build_object(
            'sub',
            'a0000000-0000-0000-0001-000000000005',
            'email',
            'logistics@harvestpro.nz'
        ),
        now(),
        now(),
        now()
    ),
    (
        'a0000000-0000-0000-0001-000000000006',
        'a0000000-0000-0000-0001-000000000006',
        'payroll@harvestpro.nz',
        'email',
        jsonb_build_object(
            'sub',
            'a0000000-0000-0000-0001-000000000006',
            'email',
            'payroll@harvestpro.nz'
        ),
        now(),
        now(),
        now()
    ),
    (
        'a0000000-0000-0000-0001-000000000007',
        'a0000000-0000-0000-0001-000000000007',
        'admin@harvestpro.nz',
        'email',
        jsonb_build_object(
            'sub',
            'a0000000-0000-0000-0001-000000000007',
            'email',
            'admin@harvestpro.nz'
        ),
        now(),
        now(),
        now()
    );
SELECT '✅ Auth identities created' AS step;
-- STEP 6: Create public.users profiles (linked to auth.users)
INSERT INTO public.users (
        id,
        email,
        full_name,
        role,
        orchard_id,
        is_active
    )
VALUES (
        'a0000000-0000-0000-0001-000000000001',
        'manager@harvestpro.nz',
        'James Wilson',
        'manager',
        'a0000000-0000-0000-0000-000000000001',
        true
    ),
    (
        'a0000000-0000-0000-0001-000000000002',
        'lead@harvestpro.nz',
        'Sarah Thompson',
        'team_leader',
        'a0000000-0000-0000-0000-000000000001',
        true
    ),
    (
        'a0000000-0000-0000-0001-000000000003',
        'runner@harvestpro.nz',
        'Liam Ngata',
        'runner',
        'a0000000-0000-0000-0000-000000000001',
        true
    ),
    (
        'a0000000-0000-0000-0001-000000000004',
        'hr@harvestpro.nz',
        'Maria González',
        'hr_admin',
        'a0000000-0000-0000-0000-000000000001',
        true
    ),
    (
        'a0000000-0000-0000-0001-000000000005',
        'logistics@harvestpro.nz',
        'Carlos Muñoz',
        'logistics',
        'a0000000-0000-0000-0000-000000000001',
        true
    ),
    (
        'a0000000-0000-0000-0001-000000000006',
        'payroll@harvestpro.nz',
        'Ana Torres',
        'payroll_admin',
        'a0000000-0000-0000-0000-000000000001',
        true
    ),
    (
        'a0000000-0000-0000-0001-000000000007',
        'admin@harvestpro.nz',
        'David Chen',
        'admin',
        'a0000000-0000-0000-0000-000000000001',
        true
    ) ON CONFLICT (id) DO
UPDATE
SET email = EXCLUDED.email,
    full_name = EXCLUDED.full_name,
    role = EXCLUDED.role,
    orchard_id = EXCLUDED.orchard_id,
    is_active = true;
SELECT '✅ Public user profiles created' AS step;
-- STEP 7: Verify
SELECT '========================================' AS separator;
SELECT 'DEMO ACCOUNTS — All Password: 111111' AS title;
SELECT u.email,
    u.full_name,
    u.role,
    u.is_active,
    CASE
        WHEN a.id IS NOT NULL THEN '✅'
        ELSE '❌'
    END AS auth_linked
FROM public.users u
    LEFT JOIN auth.users a ON a.id = u.id
ORDER BY u.role;
SELECT '========================================' AS separator;