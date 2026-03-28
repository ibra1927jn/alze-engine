-- =============================================
-- IDEMPOTENCY: Unique constraint on bucket_events.id
-- Prevents duplicate bucket inserts from network retries
-- =============================================

-- If bucket_events.id doesn't have a unique constraint, add one.
-- This ensures that if HarvestSyncBridge retries a batch (due to Wi-Fi
-- flicker), the duplicate UUIDs are rejected by the DB automatically.

-- Safe: if the constraint already exists (common on 'id' as PK), this is a no-op error.
DO $$
BEGIN
    -- Check if 'id' is already a primary key or has a unique constraint
    IF NOT EXISTS (
        SELECT 1 FROM pg_constraint
        WHERE conrelid = 'bucket_events'::regclass
        AND contype IN ('p', 'u')  -- primary key or unique
        AND array_length(conkey, 1) = 1
        AND conkey[1] = (
            SELECT attnum FROM pg_attribute
            WHERE attrelid = 'bucket_events'::regclass AND attname = 'id'
        )
    ) THEN
        ALTER TABLE bucket_events ADD CONSTRAINT bucket_events_id_unique UNIQUE (id);
        RAISE NOTICE 'Added UNIQUE constraint on bucket_events.id';
    ELSE
        RAISE NOTICE 'bucket_events.id already has a unique/primary key constraint';
    END IF;
END $$;
