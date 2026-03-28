-- =============================================
-- MIGRATION: Security Hardening
-- Date: 2026-03-03
-- Fixes: Supabase Security Advisor issues
--   1. SET search_path = '' on all public functions (prevents schema injection)
--   2. Change pickers_performance_today from SECURITY DEFINER → SECURITY INVOKER
-- =============================================
-- ============================================
-- PART 1: Fix mutable search_path on all public functions
-- The search_path attack vector: a malicious user creates a function
-- in their own schema with the same name as a built-in, and if
-- search_path includes that schema, the malicious function runs instead.
-- Fix: SET search_path = '' forces all references to be schema-qualified.
-- ============================================
-- Trigger functions (used by updated_at triggers)
ALTER FUNCTION public.auto_update_updated_at()
SET search_path = '';
ALTER FUNCTION public.update_updated_at_column()
SET search_path = '';
-- Version bump functions (used by optimistic locking)
ALTER FUNCTION public.bump_version()
SET search_path = '';
ALTER FUNCTION public.bump_version_and_update_time()
SET search_path = '';
-- Query helper functions
ALTER FUNCTION public.get_orchard_bucket_counts(uuid, date)
SET search_path = '';
ALTER FUNCTION public.get_picker_bucket_count(uuid, date)
SET search_path = '';
-- Day closure / batch processing
-- NOTE: verify signature matches your DB. Use: SELECT proname, pg_get_function_identity_arguments(oid) FROM pg_proc WHERE proname = 'update_day_closure';
-- ALTER FUNCTION public.update_day_closure(...) SET search_path = '';
-- Account management
ALTER FUNCTION public.unlock_account(text, text)
SET search_path = '';
-- ============================================
-- PART 2: Fix pickers_performance_today view
-- SECURITY DEFINER views run as the view OWNER (superuser),
-- bypassing RLS. Change to SECURITY INVOKER so the view
-- runs with the privileges of the querying user.
-- ============================================
-- Drop first because CREATE OR REPLACE can't rename columns
DROP VIEW IF EXISTS public.pickers_performance_today;
CREATE VIEW public.pickers_performance_today WITH (security_invoker = true) AS
SELECT p.id,
    p.full_name,
    p.orchard_id,
    COALESCE(count(br.id), 0::bigint) AS buckets_today,
    COALESCE(
        sum(
            CASE
                WHEN br.quality_grade = 'A' THEN 1
                ELSE 0
            END
        ),
        0::bigint
    ) AS grade_a,
    COALESCE(
        sum(
            CASE
                WHEN br.quality_grade = 'B' THEN 1
                ELSE 0
            END
        ),
        0::bigint
    ) AS grade_b,
    COALESCE(
        sum(
            CASE
                WHEN br.quality_grade = 'C' THEN 1
                ELSE 0
            END
        ),
        0::bigint
    ) AS grade_c,
    COALESCE(
        sum(
            CASE
                WHEN br.quality_grade = 'reject' THEN 1
                ELSE 0
            END
        ),
        0::bigint
    ) AS rejects
FROM public.users p
    LEFT JOIN public.bucket_records br ON br.picker_id = p.id
    AND br.scanned_at::date = CURRENT_DATE
WHERE p.role = 'picker'
    AND p.is_active = true
GROUP BY p.id,
    p.full_name,
    p.orchard_id;
-- ============================================
-- PART 3: Performance — ensure key indexes exist
-- ============================================
-- bucket_records: most queried table for dashboard/analytics
CREATE INDEX IF NOT EXISTS idx_bucket_records_scanned_at ON public.bucket_records (scanned_at);
CREATE INDEX IF NOT EXISTS idx_bucket_records_picker_orchard ON public.bucket_records (picker_id, orchard_id);
CREATE INDEX IF NOT EXISTS idx_bucket_records_quality ON public.bucket_records (quality_grade);
-- daily_attendance: queried by date range + orchard
CREATE INDEX IF NOT EXISTS idx_daily_attendance_date ON public.daily_attendance (date);
CREATE INDEX IF NOT EXISTS idx_daily_attendance_orchard_date ON public.daily_attendance (orchard_id, date);
-- users: queried by role and orchard frequently
CREATE INDEX IF NOT EXISTS idx_users_role ON public.users (role);
CREATE INDEX IF NOT EXISTS idx_users_orchard_active ON public.users (orchard_id, is_active);
-- contracts: queried by employee
CREATE INDEX IF NOT EXISTS idx_contracts_employee ON public.contracts (employee_id);
-- qc_inspections: queried by orchard
CREATE INDEX IF NOT EXISTS idx_qc_inspections_orchard ON public.qc_inspections (orchard_id);
-- messages: queried by receiver and timestamp
CREATE INDEX IF NOT EXISTS idx_messages_receiver ON public.messages (receiver_id);
CREATE INDEX IF NOT EXISTS idx_messages_created_at ON public.messages (created_at);
-- audit_logs: queried by timestamp
CREATE INDEX IF NOT EXISTS idx_audit_logs_created_at ON public.audit_logs (created_at);
-- alerts: queried by orchard
CREATE INDEX IF NOT EXISTS idx_alerts_orchard ON public.alerts (orchard_id);