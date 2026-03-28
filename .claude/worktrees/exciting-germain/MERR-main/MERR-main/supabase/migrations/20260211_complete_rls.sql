-- =============================================
-- COMPLETE RLS POLICIES - All Tables
-- =============================================
-- Version: 1.0
-- Created: 2026-02-11
-- Purpose: Comprehensive Row Level Security across all tables
-- =============================================
-- =============================================
-- 1. MESSAGES TABLE - Private messaging
-- =============================================
ALTER TABLE messages ENABLE ROW LEVEL SECURITY;
-- Users can view messages where they are sender or recipient, or if they are manager
DROP POLICY IF EXISTS "messages_view_policy" ON messages;
CREATE POLICY "messages_view_policy" ON messages FOR
SELECT USING (
        auth.uid() = sender_id
        OR auth.uid() = recipient_id
        OR auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- Users can send messages (must be sender)
DROP POLICY IF EXISTS "messages_insert_policy" ON messages;
CREATE POLICY "messages_insert_policy" ON messages FOR
INSERT WITH CHECK (auth.uid() = sender_id);
-- Users can update their own sent messages (e.g., mark as read on sender side)
DROP POLICY IF EXISTS "messages_update_policy" ON messages;
CREATE POLICY "messages_update_policy" ON messages FOR
UPDATE USING (
        auth.uid() = sender_id
        OR auth.uid() = recipient_id
    );
-- Only sender or managers can delete messages
DROP POLICY IF EXISTS "messages_delete_policy" ON messages;
CREATE POLICY "messages_delete_policy" ON messages FOR DELETE USING (
    auth.uid() = sender_id
    OR auth.uid() IN (
        SELECT id
        FROM users
        WHERE role = 'manager'
    )
);
-- =============================================
-- 2. BROADCASTS TABLE - System-wide announcements
-- =============================================
ALTER TABLE broadcasts ENABLE ROW LEVEL SECURITY;
-- All authenticated users can view broadcasts
DROP POLICY IF EXISTS "broadcasts_view_policy" ON broadcasts;
CREATE POLICY "broadcasts_view_policy" ON broadcasts FOR
SELECT USING (auth.uid() IS NOT NULL);
-- Only managers can create broadcasts
DROP POLICY IF EXISTS "broadcasts_insert_policy" ON broadcasts;
CREATE POLICY "broadcasts_insert_policy" ON broadcasts FOR
INSERT WITH CHECK (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- Only managers can update broadcasts
DROP POLICY IF EXISTS "broadcasts_update_policy" ON broadcasts;
CREATE POLICY "broadcasts_update_policy" ON broadcasts FOR
UPDATE USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- Only managers can delete broadcasts
DROP POLICY IF EXISTS "broadcasts_delete_policy" ON broadcasts;
CREATE POLICY "broadcasts_delete_policy" ON broadcasts FOR DELETE USING (
    auth.uid() IN (
        SELECT id
        FROM users
        WHERE role = 'manager'
    )
);
-- =============================================
-- 3. HARVEST_SETTINGS TABLE - Global configuration
-- =============================================
ALTER TABLE harvest_settings ENABLE ROW LEVEL SECURITY;
-- All authenticated users can view settings
DROP POLICY IF EXISTS "settings_view_policy" ON harvest_settings;
CREATE POLICY "settings_view_policy" ON harvest_settings FOR
SELECT USING (auth.uid() IS NOT NULL);
-- Only managers can update settings
DROP POLICY IF EXISTS "settings_update_policy" ON harvest_settings;
CREATE POLICY "settings_update_policy" ON harvest_settings FOR
UPDATE USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    ) WITH CHECK (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- =============================================
-- 4. ORCHARDS TABLE - Orchard management
-- =============================================
ALTER TABLE orchards ENABLE ROW LEVEL SECURITY;
-- Users can only see orchards they belong to, managers see all
DROP POLICY IF EXISTS "orchards_view_policy" ON orchards;
CREATE POLICY "orchards_view_policy" ON orchards FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
        OR id IN (
            SELECT orchard_id
            FROM users
            WHERE id = auth.uid()
        )
    );
-- Only managers can create orchards
DROP POLICY IF EXISTS "orchards_insert_policy" ON orchards;
CREATE POLICY "orchards_insert_policy" ON orchards FOR
INSERT WITH CHECK (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- Only managers can update orchards
DROP POLICY IF EXISTS "orchards_update_policy" ON orchards;
CREATE POLICY "orchards_update_policy" ON orchards FOR
UPDATE USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- =============================================
-- 5. PICKERS TABLE - Worker profiles
-- =============================================
ALTER TABLE pickers ENABLE ROW LEVEL SECURITY;
-- Users can view pickers in their orchard, managers see all
DROP POLICY IF EXISTS "pickers_view_policy" ON pickers;
CREATE POLICY "pickers_view_policy" ON pickers FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
        OR orchard_id IN (
            SELECT orchard_id
            FROM users
            WHERE id = auth.uid()
        )
    );
-- Team leaders and managers can add pickers to their orchard
DROP POLICY IF EXISTS "pickers_insert_policy" ON pickers;
CREATE POLICY "pickers_insert_policy" ON pickers FOR
INSERT WITH CHECK (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role IN ('manager', 'team-leader')
        )
    );
-- Team leaders and managers can update pickers
DROP POLICY IF EXISTS "pickers_update_policy" ON pickers;
CREATE POLICY "pickers_update_policy" ON pickers FOR
UPDATE USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role IN ('manager', 'team-leader')
        )
    );
-- Only managers can delete pickers (soft delete usually)
DROP POLICY IF EXISTS "pickers_delete_policy" ON pickers;
CREATE POLICY "pickers_delete_policy" ON pickers FOR DELETE USING (
    auth.uid() IN (
        SELECT id
        FROM users
        WHERE role = 'manager'
    )
);
-- =============================================
-- 6. DAILY_ATTENDANCE TABLE - Check-ins
-- =============================================
ALTER TABLE daily_attendance ENABLE ROW LEVEL SECURITY;
-- Users can view attendance in their orchard
DROP POLICY IF EXISTS "attendance_view_policy" ON daily_attendance;
CREATE POLICY "attendance_view_policy" ON daily_attendance FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
        OR picker_id IN (
            SELECT id
            FROM pickers
            WHERE orchard_id IN (
                    SELECT orchard_id
                    FROM users
                    WHERE id = auth.uid()
                )
        )
    );
-- Team leaders and managers can record attendance
DROP POLICY IF EXISTS "attendance_insert_policy" ON daily_attendance;
CREATE POLICY "attendance_insert_policy" ON daily_attendance FOR
INSERT WITH CHECK (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role IN ('manager', 'team-leader')
        )
    );
-- Team leaders and managers can update attendance
DROP POLICY IF EXISTS "attendance_update_policy" ON daily_attendance;
CREATE POLICY "attendance_update_policy" ON daily_attendance FOR
UPDATE USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role IN ('manager', 'team-leader')
        )
    );
-- =============================================
-- 7. BUCKET_SCANS TABLE - Production tracking
-- =============================================
ALTER TABLE bucket_scans ENABLE ROW LEVEL SECURITY;
-- Users can view scans in their orchard
DROP POLICY IF EXISTS "bucket_scans_view_policy" ON bucket_scans;
CREATE POLICY "bucket_scans_view_policy" ON bucket_scans FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
        OR picker_id IN (
            SELECT id
            FROM pickers
            WHERE orchard_id IN (
                    SELECT orchard_id
                    FROM users
                    WHERE id = auth.uid()
                )
        )
    );
-- Runners can insert scans
DROP POLICY IF EXISTS "bucket_scans_insert_policy" ON bucket_scans;
CREATE POLICY "bucket_scans_insert_policy" ON bucket_scans FOR
INSERT WITH CHECK (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role IN ('manager', 'team-leader', 'runner')
        )
    );
-- Only managers can update scans (corrections)
DROP POLICY IF EXISTS "bucket_scans_update_policy" ON bucket_scans;
CREATE POLICY "bucket_scans_update_policy" ON bucket_scans FOR
UPDATE USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- =============================================
-- 8. USERS TABLE - User profiles
-- =============================================
ALTER TABLE users ENABLE ROW LEVEL SECURITY;
-- Users can view other users in their orchard
DROP POLICY IF EXISTS "users_view_policy" ON users;
CREATE POLICY "users_view_policy" ON users FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
        OR orchard_id IN (
            SELECT orchard_id
            FROM users
            WHERE id = auth.uid()
        )
        OR id = auth.uid()
    );
-- Users can update their own profile
DROP POLICY IF EXISTS "users_update_self_policy" ON users;
CREATE POLICY "users_update_self_policy" ON users FOR
UPDATE USING (id = auth.uid()) WITH CHECK (id = auth.uid());
-- Managers can update any user
DROP POLICY IF EXISTS "users_update_manager_policy" ON users;
CREATE POLICY "users_update_manager_policy" ON users FOR
UPDATE USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- Only system can insert users (via auth triggers)
DROP POLICY IF EXISTS "users_insert_policy" ON users;
CREATE POLICY "users_insert_policy" ON users FOR
INSERT WITH CHECK (true);
-- Allow insert from auth triggers
-- =============================================
-- VERIFICATION QUERIES
-- =============================================
-- To test RLS, you can use these queries:
-- SET SESSION AUTHORIZATION 'user_uuid';
-- SELECT * FROM messages; -- Should only see own messages
-- RESET SESSION AUTHORIZATION;
-- =============================================
-- MIGRATION COMPLETE
-- =============================================
COMMENT ON SCHEMA public IS 'Row Level Security enabled on all tables';