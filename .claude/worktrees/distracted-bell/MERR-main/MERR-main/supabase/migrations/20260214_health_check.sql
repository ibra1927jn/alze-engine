-- =============================================
-- HEALTH CHECK RPC
-- =============================================
-- Version: 1.0
-- Created: 2026-02-14
-- Purpose: Database health check endpoint for monitoring
-- =============================================
CREATE OR REPLACE FUNCTION health_check() RETURNS JSONB SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE result JSONB;
table_counts JSONB;
rls_status JSONB;
BEGIN -- 1. Table row counts
SELECT jsonb_build_object(
        'users',
        (
            SELECT COUNT(*)
            FROM users
        ),
        'contracts',
        (
            SELECT COUNT(*)
            FROM contracts
        ),
        'daily_attendance',
        (
            SELECT COUNT(*)
            FROM daily_attendance
        ),
        'bucket_records',
        (
            SELECT COUNT(*)
            FROM bucket_records
        ),
        'orchards',
        (
            SELECT COUNT(*)
            FROM orchards
        ),
        'login_attempts',
        (
            SELECT COUNT(*)
            FROM login_attempts
        ),
        'account_locks',
        (
            SELECT COUNT(*)
            FROM account_locks
            WHERE locked_until > now()
                AND unlocked_at IS NULL
        ),
        'audit_logs',
        (
            SELECT COUNT(*)
            FROM audit_logs
        )
    ) INTO table_counts;
-- 2. RLS status for critical tables
SELECT jsonb_agg(
        jsonb_build_object(
            'table',
            c.relname,
            'rls_enabled',
            c.relrowsecurity
        )
    )
FROM pg_class c
    JOIN pg_namespace n ON n.oid = c.relnamespace
WHERE n.nspname = 'public'
    AND c.relkind = 'r'
    AND c.relname IN (
        'users',
        'contracts',
        'daily_attendance',
        'bucket_records',
        'orchards',
        'login_attempts',
        'account_locks',
        'audit_logs'
    ) INTO rls_status;
-- 3. Build result
result := jsonb_build_object(
    'status',
    'healthy',
    'timestamp',
    now(),
    'database',
    jsonb_build_object(
        'connected',
        true,
        'version',
        version()
    ),
    'tables',
    table_counts,
    'rls',
    COALESCE(rls_status, '[]'::jsonb),
    'active_locks',
    (
        SELECT COUNT(*)
        FROM account_locks
        WHERE locked_until > now()
            AND unlocked_at IS NULL
    )
);
RETURN result;
EXCEPTION
WHEN OTHERS THEN RETURN jsonb_build_object(
    'status',
    'unhealthy',
    'timestamp',
    now(),
    'error',
    SQLERRM
);
END;
$$;
-- Grant to authenticated users (managers will check this)
GRANT EXECUTE ON FUNCTION health_check() TO authenticated;
-- =============================================
-- MIGRATION COMPLETE
-- =============================================