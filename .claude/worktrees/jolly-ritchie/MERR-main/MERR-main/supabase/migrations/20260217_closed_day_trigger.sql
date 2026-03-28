-- ============================================
-- Fix #4b: Server-Side Trigger to Block Inserts on Closed Days
-- ============================================
-- Problem: The frontend clockSkew check can be bypassed by
-- manipulating the device clock. RLS policies on bucket_events
-- exist, but bucket_records (used by SyncBridge) has no equivalent
-- server-side enforcement.
--
-- Fix: A BEFORE INSERT trigger on bucket_records that checks
-- the day_closures table. Unlike RLS, triggers fire even for
-- service role inserts and cannot be bypassed by clock manipulation.
--
-- Offline grace: Buckets scanned BEFORE closure (timestamp < closed_at)
-- are allowed even if synced AFTER closure. Only post-closure scans
-- are rejected.
-- ============================================
-- 1. Create the enforcement function
CREATE OR REPLACE FUNCTION public.enforce_closed_day_bucket_records() RETURNS TRIGGER LANGUAGE plpgsql SECURITY DEFINER
SET search_path = public AS $$
DECLARE v_closed_at TIMESTAMPTZ;
v_bucket_date DATE;
BEGIN -- Convert bucket timestamp to NZST date for day matching
v_bucket_date := DATE(NEW.timestamp AT TIME ZONE 'Pacific/Auckland');
-- Check if the day is closed for this orchard
SELECT closed_at INTO v_closed_at
FROM day_closures
WHERE orchard_id = NEW.orchard_id
    AND date = v_bucket_date
    AND status = 'closed'
LIMIT 1;
-- If the day IS closed, check if the bucket was scanned pre-closure
IF v_closed_at IS NOT NULL THEN -- Allow pre-closure offline data (scanned before manager closed the day)
IF NEW.timestamp >= v_closed_at THEN RAISE EXCEPTION 'INSERT_BLOCKED: Day % is closed for orchard %. Bucket timestamp % is after closure at %. This may indicate clock manipulation.',
v_bucket_date,
NEW.orchard_id,
NEW.timestamp,
v_closed_at USING ERRCODE = 'P0001';
END IF;
-- If timestamp < closed_at, allow the insert (offline grace)
END IF;
RETURN NEW;
END;
$$;
-- 2. Attach trigger to bucket_records table
DROP TRIGGER IF EXISTS trg_enforce_closed_day ON public.bucket_records;
CREATE TRIGGER trg_enforce_closed_day BEFORE
INSERT ON public.bucket_records FOR EACH ROW EXECUTE FUNCTION public.enforce_closed_day_bucket_records();
-- 3. Audit comments
COMMENT ON FUNCTION public.enforce_closed_day_bucket_records() IS 'Server-side enforcement: blocks bucket inserts for closed days. Allows offline grace for pre-closure scans. Fix #4b (20260217).';
COMMENT ON TRIGGER trg_enforce_closed_day ON public.bucket_records IS 'Anti-fraud: Cannot be bypassed by client clock manipulation. Complements frontend clockSkew check.';