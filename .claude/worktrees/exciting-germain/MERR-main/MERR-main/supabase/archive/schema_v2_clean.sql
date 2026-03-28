-- ==========================================
-- HARVESTPRO NZ - SCHEMA V2 (CLEAN & CONSOLIDATED)
-- ==========================================
-- This script consolidates all previous patches and introduces the new
-- "Live Operations" architecture with Daily Attendance.
-- ==========================================

-- 1. CLEANUP & PREPARATION
-- Ensure RLS is enabled on all core tables
ALTER TABLE public.users ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.pickers ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.orchards ENABLE ROW LEVEL SECURITY;

-- 2. SECURE HELPER FUNCTIONS (Avoid Recursion)
-- These functions allow policies to check roles without triggering infinite loops on the 'users' table.

CREATE OR REPLACE FUNCTION public.get_auth_role()
RETURNS text
LANGUAGE sql
SECURITY DEFINER
SET search_path = public
AS $$
  SELECT role FROM public.users WHERE id = auth.uid();
$$;

CREATE OR REPLACE FUNCTION public.get_auth_orchard_id()
RETURNS uuid
LANGUAGE sql
SECURITY DEFINER
SET search_path = public
AS $$
  SELECT orchard_id FROM public.users WHERE id = auth.uid();
$$;

-- 3. UNIFIED RLS POLICIES

-- A. USERS TABLE (Global Directory)
DROP POLICY IF EXISTS "Global Directory Access" ON public.users;
DROP POLICY IF EXISTS "Global Directory Access for Managers" ON public.users;
DROP POLICY IF EXISTS "Managers can update users" ON public.users;

-- Read Access: Users see themselves, Managers see everyone, Peers see orchard-mates
CREATE POLICY "Global Directory Access" ON public.users
FOR SELECT USING (
    auth.uid() = id -- Self
    OR
    get_auth_role() IN ('manager', 'admin') -- Manager Global View (Module A)
    OR
    orchard_id = get_auth_orchard_id() -- Team Visibility (Module B)
);

-- Write Access: Managers can assign users (update orchard_id)
CREATE POLICY "Managers Update Users" ON public.users
FOR UPDATE USING (
  get_auth_role() IN ('manager', 'admin')
);

-- B. ORCHARDS TABLE
DROP POLICY IF EXISTS "Enable read access for authenticated users" ON public.orchards;
-- Simple read entitlement for UI selectors
CREATE POLICY "Read Orchards" ON public.orchards
FOR SELECT TO authenticated USING (true);


-- C. PICKERS (ROSTER)
DROP POLICY IF EXISTS "Read pickers" ON public.pickers;
DROP POLICY IF EXISTS "Manage pickers" ON public.pickers;
DROP POLICY IF EXISTS "Managers manage pickers" ON public.pickers;
DROP POLICY IF EXISTS "Team Leaders view own team" ON public.pickers;

-- Read Access: Managers see Global Roster. Team Leaders see THEIR crew (by ID) OR their active orchard crew.
CREATE POLICY "Read Pickers" ON public.pickers
FOR SELECT USING (
    get_auth_role() IN ('manager', 'admin') -- Module A: Global Access
    OR
    team_leader_id = auth.uid() -- My historical crew
    OR
    (orchard_id IS NOT NULL AND orchard_id = get_auth_orchard_id()) -- Module B: Live Ops Visibility
);

-- Write Access: Managers manage the roster.
CREATE POLICY "Manage Pickers" ON public.pickers
FOR ALL USING (
    get_auth_role() IN ('manager', 'admin')
    OR
    -- Allow Team Leaders to Create NEW pickers (Onboarding) but maybe strict later
    team_leader_id = auth.uid()
);


-- 4. NEW MODULE: DAILY ATTENDANCE (Live Operations Engine)
-- This table replaces "status" flags for tracking who is actually working today.

CREATE TABLE IF NOT EXISTS public.daily_attendance (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    picker_id UUID REFERENCES public.pickers(id) NOT NULL,
    orchard_id UUID REFERENCES public.orchards(id) NOT NULL,
    date DATE NOT NULL DEFAULT CURRENT_DATE,
    check_in_time TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()),
    check_out_time TIMESTAMP WITH TIME ZONE,
    status TEXT CHECK (status IN ('present', 'absent', 'sick', 'late', 'left_early')) DEFAULT 'present',
    verified_by UUID REFERENCES public.users(id),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()),
    CONSTRAINT unique_picker_daily_attendance UNIQUE (picker_id, date)
);

-- Enable RLS on Attendance
ALTER TABLE public.daily_attendance ENABLE ROW LEVEL SECURITY;

-- Attendance Policies
-- Managers: Full Access
CREATE POLICY "Managers Manage Attendance" ON public.daily_attendance
FOR ALL USING (
    get_auth_role() IN ('manager', 'admin')
);

-- Team Leaders: Can Check-in/Check-out THEIR crew or Orchard crew
CREATE POLICY "Team Leaders Manage Attendance" ON public.daily_attendance
FOR ALL USING (
    EXISTS (
        SELECT 1 FROM public.pickers p
        WHERE p.id = daily_attendance.picker_id
        AND (p.team_leader_id = auth.uid() OR p.orchard_id = get_auth_orchard_id())
    )
);

-- Runners: Read Only (To know who to scan)
CREATE POLICY "Runners Read Attendance" ON public.daily_attendance
FOR SELECT USING (
    orchard_id = get_auth_orchard_id()
);

-- Indexes for Performance
CREATE INDEX IF NOT EXISTS idx_attendance_date_orchard ON public.daily_attendance(date, orchard_id);
CREATE INDEX IF NOT EXISTS idx_attendance_picker ON public.daily_attendance(picker_id);
