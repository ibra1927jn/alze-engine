-- =============================================
-- Migration: Add updated_at to pickers and users
-- For optimistic locking (atomic WHERE pattern)
-- =============================================
-- Uses TIMESTAMPTZ(3) = millisecond precision
-- This matches JavaScript's Date.toISOString() precision
-- and prevents false conflicts from microsecond mismatch.
-- 1. PICKERS (highest mutation count — 7 update paths)
ALTER TABLE public.pickers
ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ(3) DEFAULT now();
DROP TRIGGER IF EXISTS pickers_updated_at ON public.pickers;
CREATE TRIGGER pickers_updated_at BEFORE
UPDATE ON public.pickers FOR EACH ROW EXECUTE FUNCTION set_updated_at();
-- 2. USERS (5 update paths)
ALTER TABLE public.users
ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ(3) DEFAULT now();
DROP TRIGGER IF EXISTS users_updated_at ON public.users;
CREATE TRIGGER users_updated_at BEFORE
UPDATE ON public.users FOR EACH ROW EXECUTE FUNCTION set_updated_at();
-- 3. Backfill existing rows with current timestamp
-- (rows without updated_at would fail the WHERE clause)
UPDATE public.pickers
SET updated_at = now()
WHERE updated_at IS NULL;
UPDATE public.users
SET updated_at = now()
WHERE updated_at IS NULL;