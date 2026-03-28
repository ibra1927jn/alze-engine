-- =============================================
-- SEED: All Demo Account Profiles
-- Auth users already created via API signup
-- Run this in Supabase SQL Editor
-- =============================================
-- STEP 1: Update role constraint to include ALL roles
ALTER TABLE public.users DROP CONSTRAINT IF EXISTS users_role_check;
ALTER TABLE public.users
ADD CONSTRAINT users_role_check CHECK (
        role IN (
            'manager',
            'team_leader',
            'runner',
            'qc_inspector',
            'payroll_admin',
            'admin',
            'hr_admin',
            'logistics'
        )
    );
-- STEP 2: Insert Payroll Admin profile
INSERT INTO public.users (id, email, full_name, role, is_active)
SELECT id,
    'payroll@harvestpro.nz',
    'Ana Torres',
    'payroll_admin',
    true
FROM auth.users
WHERE email = 'payroll@harvestpro.nz' ON CONFLICT (id) DO
UPDATE
SET role = 'payroll_admin',
    is_active = true;
-- STEP 3: Insert Admin profile
INSERT INTO public.users (id, email, full_name, role, is_active)
SELECT id,
    'admin@harvestpro.nz',
    'David Chen',
    'admin',
    true
FROM auth.users
WHERE email = 'admin@harvestpro.nz' ON CONFLICT (id) DO
UPDATE
SET role = 'admin',
    is_active = true;
-- STEP 4: Verify ALL demo accounts
SELECT email,
    full_name,
    role,
    is_active
FROM public.users
WHERE email IN (
        'manager@harvestpro.nz',
        'lead@harvestpro.nz',
        'runner@harvestpro.nz',
        'qc@harvestpro.nz',
        'payroll@harvestpro.nz',
        'admin@harvestpro.nz',
        'hr@harvestpro.nz',
        'logistics@harvestpro.nz'
    )
ORDER BY role;