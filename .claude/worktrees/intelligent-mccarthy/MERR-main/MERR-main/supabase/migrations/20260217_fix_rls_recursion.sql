-- ============================================
-- Fix #9: Eliminate RLS Recursion on users table
-- ============================================
-- Problem: The users_view_policy in 20260211_complete_rls.sql uses
--   SELECT id FROM users WHERE role = 'manager'
-- which triggers RLS on the same table being queried — causing
-- infinite recursion in PostgreSQL.
--
-- Fix: Create SECURITY DEFINER helper functions that bypass RLS,
-- then replace the recursive policy with these safe lookups.
-- ============================================
-- 1. SECURITY DEFINER function to get current user's role
-- Runs with OWNER privileges, bypassing RLS on users table
CREATE OR REPLACE FUNCTION public.get_auth_role() RETURNS text LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT role
FROM public.users
WHERE id = auth.uid();
$$;
-- 2. SECURITY DEFINER function to get current user's orchard_id
CREATE OR REPLACE FUNCTION public.get_auth_orchard_id() RETURNS uuid LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT orchard_id
FROM public.users
WHERE id = auth.uid();
$$;
-- 3. Revoke direct execution from anon (only authenticated should use these)
REVOKE EXECUTE ON FUNCTION public.get_auth_role()
FROM anon;
REVOKE EXECUTE ON FUNCTION public.get_auth_orchard_id()
FROM anon;
-- 4. Replace the recursive policy with safe function calls
DROP POLICY IF EXISTS "users_view_policy" ON public.users;
DROP POLICY IF EXISTS "Global Directory Access for Managers" ON public.users;
DROP POLICY IF EXISTS "Read orchard members" ON public.users;
CREATE POLICY "users_view_policy" ON public.users FOR
SELECT USING (
        id = auth.uid() -- A) Own profile (always allowed)
        OR get_auth_role() IN ('manager', 'admin') -- B) Managers/admins see all users
        OR orchard_id = get_auth_orchard_id() -- C) Same-orchard colleagues
    );
-- 5. Audit comment
COMMENT ON POLICY "users_view_policy" ON public.users IS 'Non-recursive user visibility: own profile, manager global access, same-orchard peers. Uses SECURITY DEFINER helpers to avoid RLS self-reference. Fix #9 (20260217).';
COMMENT ON FUNCTION public.get_auth_role() IS 'SECURITY DEFINER helper to read current user role without triggering RLS recursion.';
COMMENT ON FUNCTION public.get_auth_orchard_id() IS 'SECURITY DEFINER helper to read current user orchard_id without triggering RLS recursion.';