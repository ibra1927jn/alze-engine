-- =============================================
-- RATE LIMIT RPC — Atomic check + lock
-- =============================================
-- Version: 1.0
-- Created: 2026-02-14
-- Purpose: Single atomic RPC that checks rate limit state,
--   avoiding race conditions between check and lock.
-- =============================================
-- 1. Atomic rate-limit check (replaces two separate RPCs in hot path)
CREATE OR REPLACE FUNCTION check_rate_limit(check_email TEXT) RETURNS JSONB SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE active_lock RECORD;
failed_count INTEGER;
remaining INTEGER;
max_attempts CONSTANT INTEGER := 5;
lock_duration CONSTANT INTERVAL := '15 minutes';
window_duration CONSTANT INTERVAL := '15 minutes';
BEGIN -- 1. Check for active lock
SELECT locked_until INTO active_lock
FROM account_locks
WHERE email = lower(trim(check_email))
    AND locked_until > now()
    AND unlocked_at IS NULL
ORDER BY locked_at DESC
LIMIT 1;
IF FOUND THEN RETURN jsonb_build_object(
    'allowed',
    false,
    'locked',
    true,
    'locked_until',
    active_lock.locked_until,
    'remaining_ms',
    EXTRACT(
        EPOCH
        FROM (active_lock.locked_until - now())
    ) * 1000,
    'remaining_attempts',
    0
);
END IF;
-- 2. Count recent failures
SELECT COUNT(*) INTO failed_count
FROM login_attempts
WHERE email = lower(trim(check_email))
    AND success = false
    AND attempt_time > now() - window_duration;
remaining := GREATEST(0, max_attempts - failed_count);
RETURN jsonb_build_object(
    'allowed',
    true,
    'locked',
    false,
    'remaining_attempts',
    remaining,
    'failed_count',
    failed_count
);
END;
$$;
-- 2. Performance index (if not already present)
CREATE INDEX IF NOT EXISTS idx_login_attempts_email_success_time ON login_attempts(email, success, attempt_time DESC);
-- 3. Grant execute to authenticated and anon (needed for pre-login check)
GRANT EXECUTE ON FUNCTION check_rate_limit(TEXT) TO authenticated,
    anon;
-- =============================================
-- MIGRATION COMPLETE
-- =============================================