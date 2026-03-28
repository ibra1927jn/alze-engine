-- ============================================
-- FASE 4: RLS Fix for Offline Uploads on Closed Days
-- ============================================
-- Problem: The original policy blocks ALL inserts on closed days,
-- which prevents offline-synced buckets from being uploaded after
-- the manager closes the day. Field workers in low-signal zones
-- accumulate buckets locally and sync them later.
--
-- Fix: Allow inserts where recorded_at < closed_at (the bucket was
-- scanned before the day was closed, just synced late).
-- Still block inserts where recorded_at >= closed_at (post-closure).
-- ============================================

-- Drop the original strict policy
DROP POLICY IF EXISTS "no_insert_on_closed_days" ON bucket_events;

-- Recreate with offline-friendly condition
-- Uses NZST timezone for correct date matching
CREATE POLICY "no_insert_on_closed_days"
ON bucket_events
FOR INSERT
WITH CHECK (
  NOT EXISTS (
    SELECT 1 FROM day_closures
    WHERE day_closures.orchard_id = bucket_events.orchard_id
    AND day_closures.date = DATE(bucket_events.recorded_at AT TIME ZONE 'Pacific/Auckland')
    AND day_closures.status = 'closed'
    AND bucket_events.recorded_at >= day_closures.closed_at  -- Allow pre-closure offline data
  )
);

-- Comment for audit trail
COMMENT ON POLICY "no_insert_on_closed_days" ON bucket_events IS
  'Blocks post-closure inserts but allows offline-synced buckets recorded before closure time. Fase 4 legal fix.';
