-- ============================================================
-- R8-Fix4: Server-side Optimistic Concurrency Control
-- Prevents client-side bypass of updated_at checks
-- ============================================================
-- Generic trigger function: rejects UPDATE if caller's updated_at
-- does not match the current row's updated_at (stale write detection)
CREATE OR REPLACE FUNCTION check_optimistic_lock() RETURNS trigger AS $$ BEGIN -- Only enforce if the caller explicitly sends updated_at
    -- (Allows internal/admin updates that don't set updated_at)
    IF NEW.updated_at IS NOT NULL
    AND OLD.updated_at IS NOT NULL THEN IF NEW.updated_at != OLD.updated_at THEN RAISE EXCEPTION 'Optimistic lock conflict: record was modified by another user (expected %, got %)',
    OLD.updated_at,
    NEW.updated_at USING ERRCODE = '40001';
-- serialization_failure
END IF;
END IF;
-- Always bump updated_at to NOW on successful update
NEW.updated_at = NOW();
RETURN NEW;
END;
$$ LANGUAGE plpgsql;
-- Apply to critical tables that support concurrent editing
-- daily_attendance: timesheets corrected by HR + checked out by team leaders
DO $$ BEGIN IF NOT EXISTS (
    SELECT 1
    FROM pg_trigger
    WHERE tgname = 'trg_attendance_occ'
) THEN CREATE TRIGGER trg_attendance_occ BEFORE
UPDATE ON daily_attendance FOR EACH ROW EXECUTE FUNCTION check_optimistic_lock();
END IF;
END $$;
-- harvest_settings: rates modified by managers
DO $$ BEGIN IF NOT EXISTS (
    SELECT 1
    FROM pg_trigger
    WHERE tgname = 'trg_settings_occ'
) THEN CREATE TRIGGER trg_settings_occ BEFORE
UPDATE ON harvest_settings FOR EACH ROW EXECUTE FUNCTION check_optimistic_lock();
END IF;
END $$;
-- pickers: profile edits by managers/team leaders
DO $$ BEGIN IF NOT EXISTS (
    SELECT 1
    FROM pg_trigger
    WHERE tgname = 'trg_pickers_occ'
) THEN CREATE TRIGGER trg_pickers_occ BEFORE
UPDATE ON pickers FOR EACH ROW EXECUTE FUNCTION check_optimistic_lock();
END IF;
END $$;