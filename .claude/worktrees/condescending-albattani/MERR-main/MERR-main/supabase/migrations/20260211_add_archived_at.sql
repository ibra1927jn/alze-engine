-- 🔴 FASE 9: Add archived_at timestamp and index for soft delete
-- This migration adds support for soft delete of pickers instead of hard deletes
-- Prevents orphaned bucket_records and maintains historical data integrity

-- Add archived_at column to pickers table
ALTER TABLE public.pickers 
ADD COLUMN IF NOT EXISTS archived_at TIMESTAMPTZ;

-- Create index for better query performance on archived pickers
CREATE INDEX IF NOT EXISTS idx_pickers_archived_at 
ON public.pickers(archived_at) 
WHERE archived_at IS NOT NULL;

-- Update existing pickers with status='archived' to have archived_at timestamp
-- (backfill for any existing archived pickers)
UPDATE public.pickers
SET archived_at = NOW()
WHERE status = 'archived' AND archived_at IS NULL;

-- Add comment for documentation
COMMENT ON COLUMN public.pickers.archived_at IS 'Timestamp when picker was archived (soft deleted). NULL means active picker.';
