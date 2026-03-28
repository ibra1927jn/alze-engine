-- =============================================
-- SEED: Create Test Accounts for HarvestPro NZ
-- Run in Supabase SQL Editor (requires service_role access)
-- Creates 8 test accounts with password: 111111
-- =============================================
DO $$
DECLARE v_orchard_id UUID;
v_password_hash TEXT;
v_user_ids UUID [] := ARRAY [
        gen_random_uuid(), -- manager
        gen_random_uuid(), -- lead
        gen_random_uuid(), -- runner
        gen_random_uuid(), -- qc
        gen_random_uuid(), -- payroll
        gen_random_uuid(), -- admin
        gen_random_uuid(), -- hr
        gen_random_uuid()  -- logistics
    ];
v_emails TEXT [] := ARRAY [
        'manager@harvestpro.nz',
        'lead@harvestpro.nz',
        'runner@harvestpro.nz',
        'qc@harvestpro.nz',
        'payroll@harvestpro.nz',
        'admin@harvestpro.nz',
        'hr@harvestpro.nz',
        'logistics@harvestpro.nz'
    ];
v_roles TEXT [] := ARRAY [
        'manager',
        'team_leader',
        'runner',
        'qc_inspector',
        'payroll_admin',
        'admin',
        'hr_admin',
        'logistics'
    ];
v_names TEXT [] := ARRAY [
        'Manager HarvestPro',
        'Team Leader',
        'Bucket Runner',
        'QC Inspector',
        'Payroll Admin',
        'System Admin',
        'HR Admin',
        'Logistics Manager'
    ];
i INT;
BEGIN -- 1. Get the first orchard
SELECT id INTO v_orchard_id
FROM public.orchards
LIMIT 1;
IF v_orchard_id IS NULL THEN RAISE EXCEPTION 'No orchards found. Create an orchard first.';
END IF;
RAISE NOTICE 'Using orchard: %',
v_orchard_id;
-- 2. bcrypt hash for '111111' — generated with cost factor 10
-- This is the standard Supabase/GoTrue bcrypt hash for password '111111'
v_password_hash := crypt('111111', gen_salt('bf'));
-- 3. Create users in auth.users + public.users
FOR i IN 1..8 LOOP -- Skip if user already exists in auth.users
IF EXISTS (
    SELECT 1
    FROM auth.users
    WHERE email = v_emails [i]
) THEN RAISE NOTICE 'User % already exists, skipping auth creation',
v_emails [i];
-- Get the existing user ID
SELECT id INTO v_user_ids [i]
FROM auth.users
WHERE email = v_emails [i];
ELSE -- Insert into auth.users (Supabase internal auth table)
INSERT INTO auth.users (
        id,
        instance_id,
        aud,
        role,
        email,
        encrypted_password,
        email_confirmed_at,
        confirmation_sent_at,
        raw_app_meta_data,
        raw_user_meta_data,
        created_at,
        updated_at,
        is_sso_user
    )
VALUES (
        v_user_ids [i],
        '00000000-0000-0000-0000-000000000000',
        'authenticated',
        'authenticated',
        v_emails [i],
        v_password_hash,
        now(),
        -- email already confirmed!
        now(),
        jsonb_build_object('provider', 'email', 'providers', ARRAY ['email']),
        jsonb_build_object('full_name', v_names [i], 'role', v_roles [i]),
        now(),
        now(),
        false
    );
-- Insert identity for the user
INSERT INTO auth.identities (
        id,
        user_id,
        provider_id,
        identity_data,
        provider,
        last_sign_in_at,
        created_at,
        updated_at
    )
VALUES (
        gen_random_uuid(),
        v_user_ids [i],
        v_emails [i],
        jsonb_build_object('sub', v_user_ids [i]::text, 'email', v_emails [i]),
        'email',
        now(),
        now(),
        now()
    );
RAISE NOTICE 'Created auth user: % (%)',
v_emails [i],
v_roles [i];
END IF;
-- Insert into public.users (skip if already exists)
INSERT INTO public.users (
        id,
        email,
        full_name,
        role,
        orchard_id,
        is_active
    )
VALUES (
        v_user_ids [i],
        v_emails [i],
        v_names [i],
        v_roles [i],
        v_orchard_id,
        true
    ) ON CONFLICT (id) DO
UPDATE
SET email = EXCLUDED.email,
    full_name = EXCLUDED.full_name,
    role = EXCLUDED.role,
    orchard_id = EXCLUDED.orchard_id,
    is_active = true;
RAISE NOTICE 'Upserted public.users: % as %',
v_emails [i],
v_roles [i];
-- Insert into allowed_registrations (mark as used)
INSERT INTO public.allowed_registrations (email, role, orchard_id, full_name, used_at)
VALUES (
        v_emails [i],
        v_roles [i],
        v_orchard_id,
        v_names [i],
        now()
    ) ON CONFLICT (email) DO NOTHING;
END LOOP;
RAISE NOTICE '✅ All 8 test accounts created successfully!';
RAISE NOTICE 'Login with any of: manager@harvestpro.nz / 111111';
END $$;
-- Verify the accounts were created
SELECT u.email,
    u.email_confirmed_at IS NOT NULL AS confirmed,
    pu.role,
    pu.full_name,
    pu.orchard_id IS NOT NULL AS has_orchard
FROM auth.users u
    LEFT JOIN public.users pu ON pu.id = u.id
WHERE u.email LIKE '%@harvestpro.nz'
ORDER BY u.email;