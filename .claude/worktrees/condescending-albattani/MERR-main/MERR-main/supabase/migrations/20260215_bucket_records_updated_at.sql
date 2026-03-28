-- Migration: Add updated_at to bucket_records for future optimistic locking
-- Uses TIMESTAMPTZ(3) for JS millisecond precision compatibility
ALTER TABLE public.bucket_records
ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ(3) DEFAULT now();
-- Trigger to auto-update on row modification
DROP TRIGGER IF EXISTS bucket_records_updated_at ON public.bucket_records;
CREATE TRIGGER bucket_records_updated_at BEFORE
UPDATE ON public.bucket_records FOR EACH ROW EXECUTE FUNCTION set_updated_at();
-- Backfill existing rows
UPDATE public.bucket_records
SET updated_at = created_at
WHERE updated_at IS NULL;