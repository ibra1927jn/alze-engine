-- =============================================
-- 001_atomic_rpcs.sql
-- Atomic RPC functions for HarvestPro NZ
-- =============================================
-- Deploy: Copy-paste into Supabase SQL Editor → Run
-- These replace sequential frontend calls with single atomic transactions.
-- ─────────────────────────────────────────────
-- 1. setup_orchard_atomic
-- Creates orchard + day_setup in one transaction.
-- If either fails, both are rolled back.
-- ─────────────────────────────────────────────
CREATE OR REPLACE FUNCTION setup_orchard_atomic(
        p_code TEXT,
        p_name TEXT,
        p_location TEXT DEFAULT NULL,
        p_total_rows INT DEFAULT 0,
        p_start_time TEXT DEFAULT '07:00',
        p_piece_rate NUMERIC DEFAULT 6.5
    ) RETURNS JSON LANGUAGE plpgsql SECURITY DEFINER AS $$
DECLARE v_orchard RECORD;
v_today TEXT := to_char(
    NOW() AT TIME ZONE 'Pacific/Auckland',
    'YYYY-MM-DD'
);
BEGIN -- 1. Create orchard
INSERT INTO orchards (code, name, location, total_rows)
VALUES (p_code, p_name, p_location, p_total_rows)
RETURNING * INTO v_orchard;
-- 2. Create day setup with rates
INSERT INTO day_setups (orchard_id, date, start_time, piece_rate)
VALUES (
        v_orchard.id,
        v_today,
        p_start_time,
        p_piece_rate
    );
RETURN json_build_object(
    'id',
    v_orchard.id,
    'code',
    v_orchard.code,
    'name',
    v_orchard.name
);
END;
$$;
-- ─────────────────────────────────────────────
-- 2. check_in_picker
-- Insert attendance + set picker status to 'active' atomically.
-- Handles idempotency (re-check-in returns existing record).
-- ─────────────────────────────────────────────
CREATE OR REPLACE FUNCTION check_in_picker(
        p_picker_id UUID,
        p_orchard_id UUID,
        p_verified_by UUID DEFAULT NULL
    ) RETURNS JSON LANGUAGE plpgsql SECURITY DEFINER AS $$
DECLARE v_today TEXT := to_char(
        NOW() AT TIME ZONE 'Pacific/Auckland',
        'YYYY-MM-DD'
    );
v_now TEXT := to_char(
    NOW() AT TIME ZONE 'Pacific/Auckland',
    'YYYY-MM-DD"T"HH24:MI:SS'
);
v_existing RECORD;
v_new RECORD;
BEGIN -- Idempotency: check if already checked in today
SELECT id INTO v_existing
FROM daily_attendance
WHERE picker_id = p_picker_id
    AND orchard_id = p_orchard_id
    AND date = v_today
LIMIT 1;
IF v_existing.id IS NOT NULL THEN -- Already checked in — just ensure picker is active
UPDATE pickers
SET status = 'active'
WHERE id = p_picker_id;
RETURN json_build_object(
    'picker_id',
    p_picker_id,
    'status',
    'present',
    'id',
    v_existing.id
);
END IF;
-- New check-in
INSERT INTO daily_attendance (
        picker_id,
        orchard_id,
        date,
        check_in_time,
        status,
        verified_by
    )
VALUES (
        p_picker_id,
        p_orchard_id,
        v_today,
        v_now,
        'present',
        p_verified_by
    )
RETURNING * INTO v_new;
-- Set picker active
UPDATE pickers
SET status = 'active'
WHERE id = p_picker_id;
RETURN json_build_object(
    'picker_id',
    p_picker_id,
    'status',
    'present',
    'id',
    v_new.id
);
END;
$$;
-- ─────────────────────────────────────────────
-- 3. check_out_picker
-- Update attendance with check-out time + hours_worked,
-- then set picker status to 'inactive'. All atomic.
-- ─────────────────────────────────────────────
CREATE OR REPLACE FUNCTION check_out_picker(p_attendance_id UUID) RETURNS JSON LANGUAGE plpgsql SECURITY DEFINER AS $$
DECLARE v_now TEXT := to_char(
        NOW() AT TIME ZONE 'Pacific/Auckland',
        'YYYY-MM-DD"T"HH24:MI:SS'
    );
v_record RECORD;
v_hours NUMERIC;
BEGIN -- Fetch current record
SELECT * INTO v_record
FROM daily_attendance
WHERE id = p_attendance_id;
IF v_record IS NULL THEN RAISE EXCEPTION 'Attendance record not found: %',
p_attendance_id;
END IF;
-- Calculate hours worked
IF v_record.check_in_time IS NOT NULL THEN v_hours := ROUND(
    EXTRACT(
        EPOCH
        FROM (
                (v_now::TIMESTAMPTZ) - (v_record.check_in_time::TIMESTAMPTZ)
            )
    ) / 3600.0,
    2
);
v_hours := GREATEST(v_hours, 0);
END IF;
-- Update attendance
UPDATE daily_attendance
SET check_out_time = v_now,
    status = 'present',
    hours_worked = v_hours
WHERE id = p_attendance_id;
-- Set picker inactive
UPDATE pickers
SET status = 'inactive'
WHERE id = v_record.picker_id;
RETURN json_build_object(
    'id',
    p_attendance_id,
    'picker_id',
    v_record.picker_id,
    'check_out_time',
    v_now,
    'hours_worked',
    v_hours
);
END;
$$;
-- ─────────────────────────────────────────────
-- 4. correct_attendance
-- Update attendance record + create audit log entry atomically.
-- Ensures compliance trail is never missing.
-- ─────────────────────────────────────────────
CREATE OR REPLACE FUNCTION correct_attendance(
        p_attendance_id UUID,
        p_check_in_time TEXT DEFAULT NULL,
        p_check_out_time TEXT DEFAULT NULL,
        p_reason TEXT DEFAULT '',
        p_admin_id UUID DEFAULT NULL
    ) RETURNS VOID LANGUAGE plpgsql SECURITY DEFINER AS $$
DECLARE v_existing RECORD;
v_ci TEXT;
v_co TEXT;
v_hours NUMERIC;
v_now TEXT := to_char(
    NOW() AT TIME ZONE 'Pacific/Auckland',
    'YYYY-MM-DD"T"HH24:MI:SS'
);
BEGIN -- Fetch existing record for hours recalculation
SELECT check_in_time,
    check_out_time INTO v_existing
FROM daily_attendance
WHERE id = p_attendance_id;
-- Determine final check-in/out times
v_ci := COALESCE(p_check_in_time, v_existing.check_in_time);
v_co := COALESCE(p_check_out_time, v_existing.check_out_time);
-- Recalculate hours if both times available
IF v_ci IS NOT NULL
AND v_co IS NOT NULL THEN v_hours := GREATEST(
    0,
    ROUND(
        EXTRACT(
            EPOCH
            FROM (
                    (v_co::TIMESTAMPTZ) - (v_ci::TIMESTAMPTZ)
                )
        ) / 3600.0,
        2
    )
);
END IF;
-- 1. Update attendance record
UPDATE daily_attendance
SET check_in_time = COALESCE(p_check_in_time, check_in_time),
    check_out_time = COALESCE(p_check_out_time, check_out_time),
    hours_worked = COALESCE(v_hours, hours_worked),
    correction_reason = p_reason,
    corrected_by = p_admin_id,
    corrected_at = v_now
WHERE id = p_attendance_id;
-- 2. Insert audit log (atomic — never skipped)
INSERT INTO audit_logs (
        action,
        entity_type,
        entity_id,
        performed_by,
        new_values,
        notes
    )
VALUES (
        'timesheet_correction',
        'daily_attendance',
        p_attendance_id::TEXT,
        p_admin_id::TEXT,
        json_build_object(
            'check_in_time',
            p_check_in_time,
            'check_out_time',
            p_check_out_time
        ),
        p_reason
    );
END;
$$;
-- ─────────────────────────────────────────────
-- Grant access to authenticated users
-- ─────────────────────────────────────────────
GRANT EXECUTE ON FUNCTION setup_orchard_atomic TO authenticated;
GRANT EXECUTE ON FUNCTION check_in_picker TO authenticated;
GRANT EXECUTE ON FUNCTION check_out_picker TO authenticated;
GRANT EXECUTE ON FUNCTION correct_attendance TO authenticated;