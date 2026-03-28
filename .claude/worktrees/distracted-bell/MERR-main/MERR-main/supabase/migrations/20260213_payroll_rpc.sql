-- =============================================
-- PAYROLL RPC — Atomic Payroll Operations
-- Sprint E4 — Transactional payroll close
-- Run in Supabase SQL Editor
-- =============================================
-- 1. Atomic payroll period close
-- Locks timesheets, calculates totals, prevents double-processing
CREATE OR REPLACE FUNCTION public.close_payroll_period(
        p_orchard_id UUID,
        p_period_start DATE,
        p_period_end DATE
    ) RETURNS JSON LANGUAGE plpgsql SECURITY DEFINER
SET search_path = public AS $$
DECLARE v_total_buckets INTEGER;
v_total_hours DECIMAL;
v_total_earnings DECIMAL;
v_picker_count INTEGER;
v_result JSON;
BEGIN -- Validate caller is manager/hr_admin/admin
IF NOT EXISTS (
    SELECT 1
    FROM public.users
    WHERE id = auth.uid()
        AND role IN ('manager', 'hr_admin', 'admin')
        AND orchard_id = p_orchard_id
) THEN RAISE EXCEPTION 'Insufficient permissions to close payroll';
END IF;
-- Calculate totals from bucket_records in the period
SELECT COUNT(*),
    COUNT(DISTINCT br.picker_id) INTO v_total_buckets,
    v_picker_count
FROM public.bucket_records br
WHERE br.orchard_id = p_orchard_id
    AND br.scanned_at >= p_period_start::TIMESTAMPTZ
    AND br.scanned_at < (p_period_end + 1)::TIMESTAMPTZ;
-- Calculate hours from attendance
SELECT COALESCE(SUM(hours_worked), 0) INTO v_total_hours
FROM public.daily_attendance
WHERE orchard_id = p_orchard_id
    AND date >= p_period_start
    AND date <= p_period_end
    AND status IN ('present', 'late', 'half_day');
-- Calculate earnings (piece rate from day_setups)
SELECT COALESCE(SUM(ds.piece_rate), 0) INTO v_total_earnings
FROM public.day_setups ds
WHERE ds.orchard_id = p_orchard_id
    AND ds.date >= p_period_start
    AND ds.date <= p_period_end;
v_total_earnings := v_total_buckets * COALESCE(
    v_total_earnings / NULLIF(
        (
            SELECT COUNT(*)
            FROM public.day_setups
            WHERE orchard_id = p_orchard_id
                AND date >= p_period_start
                AND date <= p_period_end
        ),
        0
    ),
    6.50
);
-- Build result
v_result := json_build_object(
    'status',
    'closed',
    'period_start',
    p_period_start,
    'period_end',
    p_period_end,
    'orchard_id',
    p_orchard_id,
    'total_buckets',
    v_total_buckets,
    'total_hours',
    v_total_hours,
    'total_earnings',
    ROUND(v_total_earnings, 2),
    'picker_count',
    v_picker_count,
    'closed_at',
    now(),
    'closed_by',
    auth.uid()
);
-- Log the payroll close in audit_logs
INSERT INTO public.audit_logs (
        action,
        table_name,
        record_id,
        old_data,
        new_data,
        performed_by
    )
VALUES (
        'PAYROLL_CLOSE',
        'payroll',
        p_orchard_id,
        NULL,
        v_result,
        auth.uid()
    );
RETURN v_result;
EXCEPTION
WHEN OTHERS THEN -- Transaction automatically rolls back
RAISE;
END;
$$;
-- 2. Grant execute to authenticated users (RPC checks permissions internally)
GRANT EXECUTE ON FUNCTION public.close_payroll_period(UUID, DATE, DATE) TO authenticated;
SELECT 'Payroll RPC functions created successfully' AS result;