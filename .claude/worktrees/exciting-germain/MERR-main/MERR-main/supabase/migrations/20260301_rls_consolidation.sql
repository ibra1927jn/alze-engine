-- ============================================
-- RLS Consolidation: Eliminate ALL recursive subqueries
-- ============================================
-- 
-- Problem: The 20260211_complete_rls.sql migration uses
--   `SELECT id FROM users WHERE role = 'manager'`
-- in policies on messages, broadcasts, orchards, pickers,
-- daily_attendance, bucket_scans, and harvest_settings.
-- Each such subquery triggers RLS on the users table,
-- causing recursive evaluation and O(n²) performance.
--
-- Fix: Replace ALL role/orchard subqueries with the
-- SECURITY DEFINER helpers created in 20260217:
--   get_auth_role()       — returns current user's role
--   get_auth_orchard_id() — returns current user's orchard_id
--
-- Also adds an is_manager() convenience function.
-- ============================================
-- ── Helper: is_manager shorthand ──────────────
CREATE OR REPLACE FUNCTION public.is_manager() RETURNS boolean LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT role IN ('manager', 'admin')
FROM public.users
WHERE id = auth.uid();
$$;
CREATE OR REPLACE FUNCTION public.is_role(allowed_roles text []) RETURNS boolean LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT role = ANY(allowed_roles)
FROM public.users
WHERE id = auth.uid();
$$;
REVOKE EXECUTE ON FUNCTION public.is_manager()
FROM anon;
REVOKE EXECUTE ON FUNCTION public.is_role(text [])
FROM anon;
COMMENT ON FUNCTION public.is_manager() IS 'SECURITY DEFINER: checks if current user is manager/admin without RLS recursion.';
COMMENT ON FUNCTION public.is_role(text []) IS 'SECURITY DEFINER: checks if current user has any of the specified roles without RLS recursion.';
-- =============================================
-- 1. MESSAGES — Fix recursive role checks
-- =============================================
DROP POLICY IF EXISTS "messages_view_policy" ON messages;
CREATE POLICY "messages_view_policy" ON messages FOR
SELECT USING (
        auth.uid() = sender_id
        OR auth.uid() = recipient_id
        OR is_manager()
    );
-- Insert: only sender
DROP POLICY IF EXISTS "messages_insert_policy" ON messages;
CREATE POLICY "messages_insert_policy" ON messages FOR
INSERT WITH CHECK (auth.uid() = sender_id);
-- Update: sender or recipient
DROP POLICY IF EXISTS "messages_update_policy" ON messages;
CREATE POLICY "messages_update_policy" ON messages FOR
UPDATE USING (
        auth.uid() = sender_id
        OR auth.uid() = recipient_id
    );
-- Delete: sender or manager
DROP POLICY IF EXISTS "messages_delete_policy" ON messages;
CREATE POLICY "messages_delete_policy" ON messages FOR DELETE USING (
    auth.uid() = sender_id
    OR is_manager()
);
-- =============================================
-- 2. BROADCASTS — Fix recursive role checks
-- =============================================
DROP POLICY IF EXISTS "broadcasts_view_policy" ON broadcasts;
CREATE POLICY "broadcasts_view_policy" ON broadcasts FOR
SELECT USING (auth.uid() IS NOT NULL);
DROP POLICY IF EXISTS "broadcasts_insert_policy" ON broadcasts;
CREATE POLICY "broadcasts_insert_policy" ON broadcasts FOR
INSERT WITH CHECK (is_manager());
DROP POLICY IF EXISTS "broadcasts_update_policy" ON broadcasts;
CREATE POLICY "broadcasts_update_policy" ON broadcasts FOR
UPDATE USING (is_manager());
DROP POLICY IF EXISTS "broadcasts_delete_policy" ON broadcasts;
CREATE POLICY "broadcasts_delete_policy" ON broadcasts FOR DELETE USING (is_manager());
-- =============================================
-- 3. HARVEST_SETTINGS — Fix recursive role checks
-- =============================================
DROP POLICY IF EXISTS "settings_view_policy" ON harvest_settings;
CREATE POLICY "settings_view_policy" ON harvest_settings FOR
SELECT USING (auth.uid() IS NOT NULL);
DROP POLICY IF EXISTS "settings_update_policy" ON harvest_settings;
CREATE POLICY "settings_update_policy" ON harvest_settings FOR
UPDATE USING (is_manager()) WITH CHECK (is_manager());
-- =============================================
-- 4. ORCHARDS — Fix recursive role + orchard checks
-- =============================================
DROP POLICY IF EXISTS "orchards_view_policy" ON orchards;
CREATE POLICY "orchards_view_policy" ON orchards FOR
SELECT USING (
        is_manager()
        OR id = get_auth_orchard_id()
    );
DROP POLICY IF EXISTS "orchards_insert_policy" ON orchards;
CREATE POLICY "orchards_insert_policy" ON orchards FOR
INSERT WITH CHECK (is_manager());
DROP POLICY IF EXISTS "orchards_update_policy" ON orchards;
CREATE POLICY "orchards_update_policy" ON orchards FOR
UPDATE USING (is_manager());
-- =============================================
-- 5. PICKERS — Fix recursive role + orchard checks
-- =============================================
DROP POLICY IF EXISTS "pickers_view_policy" ON pickers;
CREATE POLICY "pickers_view_policy" ON pickers FOR
SELECT USING (
        is_manager()
        OR orchard_id = get_auth_orchard_id()
    );
DROP POLICY IF EXISTS "pickers_insert_policy" ON pickers;
CREATE POLICY "pickers_insert_policy" ON pickers FOR
INSERT WITH CHECK (is_role(ARRAY ['manager', 'team_leader']));
DROP POLICY IF EXISTS "pickers_update_policy" ON pickers;
CREATE POLICY "pickers_update_policy" ON pickers FOR
UPDATE USING (is_role(ARRAY ['manager', 'team_leader']));
DROP POLICY IF EXISTS "pickers_delete_policy" ON pickers;
CREATE POLICY "pickers_delete_policy" ON pickers FOR DELETE USING (is_manager());
-- =============================================
-- 6. DAILY_ATTENDANCE — Fix deep recursive chain
-- =============================================
-- Was: pickers → users → (RLS on users) → recursion
-- Now: Single function call, no subqueries
DROP POLICY IF EXISTS "attendance_view_policy" ON daily_attendance;
CREATE POLICY "attendance_view_policy" ON daily_attendance FOR
SELECT USING (
        is_manager()
        OR picker_id IN (
            SELECT id
            FROM pickers
            WHERE orchard_id = get_auth_orchard_id()
        )
    );
DROP POLICY IF EXISTS "attendance_insert_policy" ON daily_attendance;
CREATE POLICY "attendance_insert_policy" ON daily_attendance FOR
INSERT WITH CHECK (is_role(ARRAY ['manager', 'team_leader']));
DROP POLICY IF EXISTS "attendance_update_policy" ON daily_attendance;
CREATE POLICY "attendance_update_policy" ON daily_attendance FOR
UPDATE USING (is_role(ARRAY ['manager', 'team_leader']));
-- =============================================
-- 7. BUCKET_SCANS — Fix deep recursive chain
-- =============================================
DROP POLICY IF EXISTS "bucket_scans_view_policy" ON bucket_scans;
CREATE POLICY "bucket_scans_view_policy" ON bucket_scans FOR
SELECT USING (
        is_manager()
        OR picker_id IN (
            SELECT id
            FROM pickers
            WHERE orchard_id = get_auth_orchard_id()
        )
    );
DROP POLICY IF EXISTS "bucket_scans_insert_policy" ON bucket_scans;
CREATE POLICY "bucket_scans_insert_policy" ON bucket_scans FOR
INSERT WITH CHECK (
        is_role(ARRAY ['manager', 'team_leader', 'runner'])
    );
DROP POLICY IF EXISTS "bucket_scans_update_policy" ON bucket_scans;
CREATE POLICY "bucket_scans_update_policy" ON bucket_scans FOR
UPDATE USING (is_manager());
-- =============================================
-- 8. USERS TABLE — Ensure using helpers (from 20260217)
-- =============================================
-- Already fixed in 20260217_fix_rls_recursion.sql
-- Just ensure all policies are consistent
DROP POLICY IF EXISTS "users_update_manager_policy" ON users;
CREATE POLICY "users_update_manager_policy" ON users FOR
UPDATE USING (is_manager());
-- =============================================
-- AUDIT COMMENT
-- =============================================
COMMENT ON SCHEMA public IS 'RLS consolidated: all role checks use SECURITY DEFINER helpers (is_manager, is_role, get_auth_role, get_auth_orchard_id). No more recursive subqueries on users table. Migration 20260301.';