-- =============================================
-- AUTH HARDENING - Rate Limiting & Account Lockout
-- =============================================
-- Version: 1.0
-- Created: 2026-02-11
-- Purpose: Prevent brute force attacks with rate limiting
-- =============================================
-- =============================================
-- 1. CREATE LOGIN_ATTEMPTS TABLE
-- =============================================
CREATE TABLE IF NOT EXISTS login_attempts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email TEXT NOT NULL,
    ip_address INET,
    attempt_time TIMESTAMPTZ DEFAULT now(),
    success BOOLEAN DEFAULT false,
    user_agent TEXT,
    failure_reason TEXT
);
-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_login_attempts_email_time ON login_attempts(email, attempt_time DESC);
CREATE INDEX IF NOT EXISTS idx_login_attempts_ip_time ON login_attempts(ip_address, attempt_time DESC);
-- Enable RLS (public insert for login tracking, managers can view)
ALTER TABLE login_attempts ENABLE ROW LEVEL SECURITY;
CREATE POLICY "anyone_can_insert_login_attempts" ON login_attempts FOR
INSERT WITH CHECK (true);
CREATE POLICY "managers_view_login_attempts" ON login_attempts FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- =============================================
-- 2. CREATE ACCOUNT_LOCKS TABLE
-- =============================================
CREATE TABLE IF NOT EXISTS account_locks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES auth.users(id) ON DELETE CASCADE,
    email TEXT NOT NULL,
    locked_at TIMESTAMPTZ DEFAULT now(),
    locked_until TIMESTAMPTZ NOT NULL,
    locked_by_system BOOLEAN DEFAULT true,
    unlock_reason TEXT,
    unlocked_by UUID REFERENCES auth.users(id),
    unlocked_at TIMESTAMPTZ,
    -- Ensure only one active lock per user
    CONSTRAINT unique_active_lock UNIQUE (user_id, locked_at)
);
-- Create indexes
CREATE INDEX IF NOT EXISTS idx_account_locks_user ON account_locks(user_id, locked_until DESC);
CREATE INDEX IF NOT EXISTS idx_account_locks_email ON account_locks(email, locked_until DESC);
-- Enable RLS
ALTER TABLE account_locks ENABLE ROW LEVEL SECURITY;
CREATE POLICY "managers_full_access_account_locks" ON account_locks FOR ALL USING (
    auth.uid() IN (
        SELECT id
        FROM users
        WHERE role = 'manager'
    )
);
CREATE POLICY "system_insert_account_locks" ON account_locks FOR
INSERT WITH CHECK (locked_by_system = true);
-- =============================================
-- 3. HELPER FUNCTIONS
-- =============================================
-- Check if account is currently locked
CREATE OR REPLACE FUNCTION is_account_locked(check_email TEXT) RETURNS BOOLEAN SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE active_lock RECORD;
BEGIN -- Find active lock (locked_until in future and not unlocked)
SELECT * INTO active_lock
FROM account_locks
WHERE email = check_email
    AND locked_until > now()
    AND unlocked_at IS NULL
ORDER BY locked_at DESC
LIMIT 1;
RETURN FOUND;
END;
$$;
-- Get failed login count in last 15 minutes
CREATE OR REPLACE FUNCTION get_failed_login_count(check_email TEXT) RETURNS INTEGER SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE failed_count INTEGER;
BEGIN
SELECT COUNT(*) INTO failed_count
FROM login_attempts
WHERE email = check_email
    AND success = false
    AND attempt_time > now() - INTERVAL '15 minutes';
RETURN failed_count;
END;
$$;
-- Lock account after too many failed attempts
CREATE OR REPLACE FUNCTION lock_account_on_failures() RETURNS TRIGGER SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE failed_count INTEGER;
user_uuid UUID;
BEGIN -- Only process failed login attempts
IF NEW.success = true THEN RETURN NEW;
END IF;
-- Count recent failures
SELECT COUNT(*) INTO failed_count
FROM login_attempts
WHERE email = NEW.email
    AND success = false
    AND attempt_time > now() - INTERVAL '15 minutes';
-- Lock account if >= 5 failures
IF failed_count >= 5 THEN -- Get user_id if exists
SELECT id INTO user_uuid
FROM auth.users
WHERE email = NEW.email;
-- Create lock (15 minute duration)
INSERT INTO account_locks (user_id, email, locked_until)
VALUES (
        user_uuid,
        NEW.email,
        now() + INTERVAL '15 minutes'
    ) ON CONFLICT (user_id, locked_at) DO NOTHING;
-- Log audit event
INSERT INTO audit_logs (
        user_id,
        user_email,
        action,
        table_name,
        record_id,
        new_values
    )
VALUES (
        user_uuid,
        NEW.email,
        'CUSTOM',
        'account_locks',
        NULL,
        jsonb_build_object(
            'reason',
            'Too many failed login attempts',
            'failed_count',
            failed_count,
            'locked_until',
            now() + INTERVAL '15 minutes'
        )
    );
END IF;
RETURN NEW;
END;
$$;
-- Apply trigger
DROP TRIGGER IF EXISTS trigger_lock_account ON login_attempts;
CREATE TRIGGER trigger_lock_account
AFTER
INSERT ON login_attempts FOR EACH ROW EXECUTE FUNCTION lock_account_on_failures();
-- =============================================
-- 4. UNLOCK FUNCTION (for Managers)
-- =============================================
CREATE OR REPLACE FUNCTION unlock_account(
        target_email TEXT,
        unlock_reason_text TEXT DEFAULT 'Unlocked by manager'
    ) RETURNS BOOLEAN SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE manager_role TEXT;
lock_record RECORD;
BEGIN -- Verify caller is a manager
SELECT role INTO manager_role
FROM users
WHERE id = auth.uid();
IF manager_role != 'manager' THEN RAISE EXCEPTION 'Only managers can unlock accounts';
END IF;
-- Find active lock
SELECT * INTO lock_record
FROM account_locks
WHERE email = target_email
    AND locked_until > now()
    AND unlocked_at IS NULL
ORDER BY locked_at DESC
LIMIT 1;
IF NOT FOUND THEN RETURN false;
-- No active lock found
END IF;
-- Unlock the account
UPDATE account_locks
SET unlocked_at = now(),
    unlocked_by = auth.uid(),
    unlock_reason = unlock_reason_text
WHERE id = lock_record.id;
-- Clear failed login attempts for this user
DELETE FROM login_attempts
WHERE email = target_email
    AND success = false
    AND attempt_time > now() - INTERVAL '15 minutes';
-- Log audit event
INSERT INTO audit_logs (
        user_id,
        user_email,
        action,
        table_name,
        record_id,
        new_values
    )
VALUES (
        lock_record.user_id,
        target_email,
        'CUSTOM',
        'account_locks',
        lock_record.id,
        jsonb_build_object(
            'action',
            'manual_unlock',
            'unlocked_by',
            auth.uid(),
            'reason',
            unlock_reason_text
        )
    );
RETURN true;
END;
$$;
-- =============================================
-- 5. CLEANUP OLD DATA (Retention Policy)
-- =============================================
-- Auto-delete login attempts older than 30 days
CREATE OR REPLACE FUNCTION cleanup_old_login_attempts() RETURNS void SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$ BEGIN
DELETE FROM login_attempts
WHERE attempt_time < now() - INTERVAL '30 days';
END;
$$;
-- Auto-delete expired locks older than 90 days
CREATE OR REPLACE FUNCTION cleanup_old_account_locks() RETURNS void SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$ BEGIN
DELETE FROM account_locks
WHERE locked_until < now() - INTERVAL '90 days';
END;
$$;
-- Note: To enable automatic cleanup, install pg_cron extension and schedule:
-- SELECT cron.schedule('cleanup-auth-data', '0 3 * * *', 
--   'SELECT cleanup_old_login_attempts(); SELECT cleanup_old_account_locks();');
-- =============================================
-- MIGRATION COMPLETE
-- =============================================