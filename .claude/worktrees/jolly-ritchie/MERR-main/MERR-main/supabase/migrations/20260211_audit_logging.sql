-- =============================================
-- AUDIT LOGGING SYSTEM
-- =============================================
-- Version: 1.0
-- Created: 2026-02-11
-- Purpose: Complete audit trail for compliance and security
-- =============================================
-- =============================================
-- 1. CREATE AUDIT_LOGS TABLE
-- =============================================
CREATE TABLE IF NOT EXISTS audit_logs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES auth.users(id),
    user_email TEXT,
    action TEXT NOT NULL CHECK (
        action IN ('INSERT', 'UPDATE', 'DELETE', 'CUSTOM')
    ),
    table_name TEXT NOT NULL,
    record_id UUID,
    old_values JSONB,
    new_values JSONB,
    ip_address INET,
    user_agent TEXT,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- Add comment
COMMENT ON TABLE audit_logs IS 'Complete audit trail for compliance and security. Logs all critical data changes.';
COMMENT ON COLUMN audit_logs.user_id IS 'User who performed the action (null for system actions)';
COMMENT ON COLUMN audit_logs.action IS 'Type of database operation: INSERT, UPDATE, DELETE, or CUSTOM';
COMMENT ON COLUMN audit_logs.table_name IS 'Table that was modified';
COMMENT ON COLUMN audit_logs.record_id IS 'ID of the record that was modified';
COMMENT ON COLUMN audit_logs.old_values IS 'JSONB of the record before change (for UPDATE/DELETE)';
COMMENT ON COLUMN audit_logs.new_values IS 'JSONB of the record after change (for INSERT/UPDATE)';
-- =============================================
-- 2. CREATE INDEXES FOR PERFORMANCE
-- =============================================
-- Query by user and time (most common query pattern)
CREATE INDEX IF NOT EXISTS idx_audit_logs_user_time ON audit_logs(user_id, created_at DESC);
-- Query by table and record (for record history)
CREATE INDEX IF NOT EXISTS idx_audit_logs_table_record ON audit_logs(table_name, record_id);
-- Query by date (for compliance reports)
CREATE INDEX IF NOT EXISTS idx_audit_logs_created_at ON audit_logs(created_at DESC);
-- Query by action type (for specific audits)
CREATE INDEX IF NOT EXISTS idx_audit_logs_action ON audit_logs(action);
-- =============================================
-- 3. ROW LEVEL SECURITY
-- =============================================
-- Enable RLS - only managers can view audit logs
ALTER TABLE audit_logs ENABLE ROW LEVEL SECURITY;
-- Policy: Only managers can view audit logs
CREATE POLICY "managers_view_audit_logs" ON audit_logs FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- Policy: System can insert audit logs (SECURITY DEFINER function)
CREATE POLICY "system_insert_audit_logs" ON audit_logs FOR
INSERT WITH CHECK (true);
-- =============================================
-- 4. AUTOMATIC AUDIT TRIGGER FUNCTION
-- =============================================
CREATE OR REPLACE FUNCTION log_audit_trail() RETURNS TRIGGER SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE current_user_email TEXT;
BEGIN -- Get current user email from auth.users
SELECT email INTO current_user_email
FROM auth.users
WHERE id = auth.uid();
-- Insert audit log entry
INSERT INTO audit_logs (
        user_id,
        user_email,
        action,
        table_name,
        record_id,
        old_values,
        new_values
    )
VALUES (
        auth.uid(),
        current_user_email,
        TG_OP,
        TG_TABLE_NAME,
        COALESCE(NEW.id, OLD.id),
        -- For UPDATE and DELETE, capture old values
        CASE
            WHEN TG_OP IN ('UPDATE', 'DELETE') THEN row_to_json(OLD)::jsonb
            ELSE NULL
        END,
        -- For INSERT and UPDATE, capture new values
        CASE
            WHEN TG_OP IN ('INSERT', 'UPDATE') THEN row_to_json(NEW)::jsonb
            ELSE NULL
        END
    );
-- Always return the appropriate record
RETURN COALESCE(NEW, OLD);
EXCEPTION
WHEN OTHERS THEN -- Log error but don't fail the original operation
RAISE WARNING 'Audit logging failed: %',
SQLERRM;
RETURN COALESCE(NEW, OLD);
END;
$$;
COMMENT ON FUNCTION log_audit_trail() IS 'Trigger function to automatically log changes to audited tables. SECURITY DEFINER ensures it runs with elevated privileges.';
-- =============================================
-- 5. APPLY TRIGGERS TO CRITICAL TABLES
-- =============================================
-- Pickers table audit
DROP TRIGGER IF EXISTS audit_pickers ON pickers;
CREATE TRIGGER audit_pickers
AFTER
INSERT
    OR
UPDATE
    OR DELETE ON pickers FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
-- Settings table audit (only for updates - critical configuration changes)
DROP TRIGGER IF EXISTS audit_settings ON settings;
CREATE TRIGGER audit_settings
AFTER
UPDATE ON settings FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
-- Users table audit (role changes, profile updates)
DROP TRIGGER IF EXISTS audit_users ON users;
CREATE TRIGGER audit_users
AFTER
UPDATE ON users FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
-- Daily attendance audit
DROP TRIGGER IF EXISTS audit_daily_attendance ON daily_attendance;
CREATE TRIGGER audit_daily_attendance
AFTER
INSERT
    OR
UPDATE
    OR DELETE ON daily_attendance FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
-- Orchards audit (only for updates - orchard configuration)
DROP TRIGGER IF EXISTS audit_orchards ON orchards;
CREATE TRIGGER audit_orchards
AFTER
UPDATE ON orchards FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
-- =============================================
-- 6. AUDIT LOG RETENTION POLICY
-- =============================================
-- Function to clean up old audit logs (90 day retention)
CREATE OR REPLACE FUNCTION cleanup_old_audit_logs() RETURNS void LANGUAGE plpgsql SECURITY DEFINER AS $$ BEGIN
DELETE FROM audit_logs
WHERE created_at < NOW() - INTERVAL '90 days';
RAISE NOTICE 'Cleaned up audit logs older than 90 days';
END;
$$;
COMMENT ON FUNCTION cleanup_old_audit_logs() IS 'Deletes audit logs older than 90 days. Should be run via cron job (e.g., pg_cron extension or external scheduler).';
-- =============================================
-- 7. HELPER FUNCTIONS
-- =============================================
-- Get audit trail for a specific record
CREATE OR REPLACE FUNCTION get_record_audit_trail(p_table_name TEXT, p_record_id UUID) RETURNS TABLE (
        id UUID,
        action TEXT,
        user_email TEXT,
        old_values JSONB,
        new_values JSONB,
        created_at TIMESTAMPTZ
    ) LANGUAGE plpgsql SECURITY DEFINER AS $$ BEGIN RETURN QUERY
SELECT a.id,
    a.action,
    a.user_email,
    a.old_values,
    a.new_values,
    a.created_at
FROM audit_logs a
WHERE a.table_name = p_table_name
    AND a.record_id = p_record_id
ORDER BY a.created_at DESC;
END;
$$;
COMMENT ON FUNCTION get_record_audit_trail IS 'Get complete audit history for a specific record';
-- =============================================
-- 8. VERIFICATION
-- =============================================
-- Verify audit system is working
DO $$ BEGIN RAISE NOTICE 'Audit logging system installed successfully!';
RAISE NOTICE 'Triggers created for: pickers, settings, users, daily_attendance, orchards';
RAISE NOTICE 'Retention policy: 90 days';
RAISE NOTICE 'Only managers can view audit logs via RLS';
END $$;