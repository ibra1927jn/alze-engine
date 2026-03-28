-- =============================================
-- DAILY ATTENDANCE TABLE
-- Sprint E1 — Picker attendance tracking
-- Run in Supabase SQL Editor
-- =============================================
-- 1. Create daily_attendance table (or add missing columns if it already exists)
CREATE TABLE IF NOT EXISTS public.daily_attendance (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    picker_id UUID NOT NULL REFERENCES public.pickers(id) ON DELETE CASCADE,
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    date DATE NOT NULL DEFAULT CURRENT_DATE,
    check_in TIMESTAMPTZ,
    check_out TIMESTAMPTZ,
    status TEXT NOT NULL DEFAULT 'present' CHECK (
        status IN (
            'present',
            'absent',
            'late',
            'half_day',
            'excused'
        )
    ),
    hours_worked DECIMAL(4, 2) DEFAULT 0,
    notes TEXT,
    recorded_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now(),
    -- One attendance record per picker per day
    CONSTRAINT daily_attendance_unique UNIQUE (picker_id, date),
    -- check_out must be after check_in
    CONSTRAINT daily_attendance_time_range CHECK (
        check_out IS NULL
        OR check_in IS NULL
        OR check_out > check_in
    )
);
-- 1b. If table already existed, ensure all columns are present
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS check_in TIMESTAMPTZ;
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS check_out TIMESTAMPTZ;
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS status TEXT DEFAULT 'present';
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS hours_worked DECIMAL(4, 2) DEFAULT 0;
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS notes TEXT;
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS recorded_by UUID;
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ DEFAULT now();
-- 2. Row Level Security
ALTER TABLE public.daily_attendance ENABLE ROW LEVEL SECURITY;
-- Read: same orchard
DROP POLICY IF EXISTS "Read attendance" ON public.daily_attendance;
CREATE POLICY "Read attendance" ON public.daily_attendance FOR
SELECT USING (orchard_id = get_my_orchard_id());
-- Write: managers, TLs, and HR can manage attendance
DROP POLICY IF EXISTS "Manage attendance" ON public.daily_attendance;
CREATE POLICY "Manage attendance" ON public.daily_attendance FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'team_leader', 'hr_admin', 'admin')
    )
);
-- 3. Performance Indexes
CREATE INDEX IF NOT EXISTS idx_attendance_orchard_date ON public.daily_attendance(orchard_id, date);
CREATE INDEX IF NOT EXISTS idx_attendance_picker_date ON public.daily_attendance(picker_id, date);
CREATE INDEX IF NOT EXISTS idx_attendance_date ON public.daily_attendance(date DESC);
CREATE INDEX IF NOT EXISTS idx_attendance_status ON public.daily_attendance(status)
WHERE status != 'present';
-- 4. Updated_at trigger
DROP TRIGGER IF EXISTS attendance_updated_at ON public.daily_attendance;
CREATE TRIGGER attendance_updated_at BEFORE
UPDATE ON public.daily_attendance FOR EACH ROW EXECUTE FUNCTION set_updated_at();
-- 5. Realtime
DO $$ BEGIN ALTER PUBLICATION supabase_realtime
ADD TABLE public.daily_attendance;
EXCEPTION
WHEN OTHERS THEN NULL;
END $$;
SELECT 'daily_attendance table created successfully' AS result;