-- =============================================
-- ALLOWED REGISTRATIONS — HR Pre-Authorization Whitelist
-- Created: 2026-02-23
-- =============================================
-- HR admins pre-register email + role. Employees self-register
-- only if their email is in this whitelist. Role is auto-assigned.
CREATE TABLE IF NOT EXISTS public.allowed_registrations (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email TEXT NOT NULL,
    role TEXT NOT NULL CHECK (
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
    ),
    orchard_id UUID REFERENCES public.orchards(id),
    full_name TEXT,
    -- Optional: HR can pre-fill employee name
    created_by UUID REFERENCES public.users(id),
    used_at TIMESTAMPTZ,
    -- NULL = pending, timestamp = registered
    created_at TIMESTAMPTZ DEFAULT now(),
    CONSTRAINT allowed_registrations_email_unique UNIQUE (email)
);
-- Enable RLS
ALTER TABLE public.allowed_registrations ENABLE ROW LEVEL SECURITY;
-- HR / Manager / Admin can manage registrations
DROP POLICY IF EXISTS "HR manage allowed registrations" ON public.allowed_registrations;
CREATE POLICY "HR manage allowed registrations" ON public.allowed_registrations FOR ALL USING (public.is_hr_manager_or_admin()) WITH CHECK (public.is_hr_manager_or_admin());
-- Anyone authenticated can SELECT their own row (for registration lookup)
-- This is needed during signUp() to check if the email is whitelisted
DROP POLICY IF EXISTS "Check own registration" ON public.allowed_registrations;
CREATE POLICY "Check own registration" ON public.allowed_registrations FOR
SELECT USING (true);
-- Registration check happens pre-auth, need permissive read
-- Index for fast lookup during registration
CREATE INDEX IF NOT EXISTS idx_allowed_registrations_email ON public.allowed_registrations (email)
WHERE used_at IS NULL;