-- =============================================
-- FIX_SCHEMA_DIVERGENCE.SQL
-- Objective: Harmonize DB schema with Frontend code & User Request
-- =============================================

-- 1. SYSTEM USER SETUP
-- Used for automated check-ins and system messages
DO $$
BEGIN
    -- Temporary bypass FK to auth.users if not present
    SET session_replication_role = 'replica';

    IF NOT EXISTS (SELECT 1 FROM public.users WHERE id = '00000000-0000-0000-0000-000000000000') THEN
        INSERT INTO public.users (id, full_name, role, is_active)
        VALUES ('00000000-0000-0000-0000-000000000000', 'System', 'manager', true);
    END IF;

    SET session_replication_role = 'origin';
END $$;


-- 2. HARVEST SETTINGS TABLE
CREATE TABLE IF NOT EXISTS public.harvest_settings (
    orchard_id UUID PRIMARY KEY REFERENCES public.orchards(id) ON DELETE CASCADE,
    min_wage_rate DECIMAL(10,2) DEFAULT 23.50,
    piece_rate DECIMAL(10,2) DEFAULT 6.50,
    min_buckets_per_hour DECIMAL(10,2) DEFAULT 3.6,
    target_tons DECIMAL(10,2) DEFAULT 40.0,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- Seed defaults for ALL orchards that lack settings (Fixes 406 errors)
INSERT INTO public.harvest_settings (orchard_id, min_wage_rate, piece_rate, min_buckets_per_hour, target_tons)
SELECT id, 23.50, 6.50, 3.6, 40.0
FROM public.orchards
ON CONFLICT (orchard_id) DO NOTHING;

-- Enable RLS
ALTER TABLE public.harvest_settings ENABLE ROW LEVEL SECURITY;

-- Policies for settings
DROP POLICY IF EXISTS "Read Harvest Settings" ON public.harvest_settings;
CREATE POLICY "Read Harvest Settings" ON public.harvest_settings
    FOR SELECT TO authenticated USING (true);

DROP POLICY IF EXISTS "Manage Harvest Settings" ON public.harvest_settings;
CREATE POLICY "Manage Harvest Settings" ON public.harvest_settings
    FOR ALL USING (
        EXISTS (
            SELECT 1 FROM public.users 
            WHERE id = auth.uid() AND role IN ('manager', 'admin')
        )
    );


-- 3. SIMPLIFIED MESSAGES TABLE (System Notifications)
CREATE TABLE IF NOT EXISTS public.messages (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    sender_id UUID NOT NULL, -- Allowed to be '000...000'
    receiver_id UUID NOT NULL REFERENCES public.users(id),
    content TEXT NOT NULL,
    type TEXT DEFAULT 'system',
    read BOOLEAN DEFAULT false,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- Note: We don't enforce FK on sender_id to auth.users to allow System UUID zero
-- unless you explicitly want to register UUID zero in auth.users.

-- Enable RLS
ALTER TABLE public.messages ENABLE ROW LEVEL SECURITY;

-- Policies for messages
DROP POLICY IF EXISTS "Users can read own messages" ON public.messages;
CREATE POLICY "Users can read own messages" ON public.messages
    FOR SELECT USING (auth.uid() = receiver_id);

DROP POLICY IF EXISTS "Allow system and managers to send" ON public.messages;
CREATE POLICY "Allow system and managers to send" ON public.messages
    FOR INSERT WITH CHECK (
        auth.uid() = sender_id 
        OR 
        EXISTS (SELECT 1 FROM public.users WHERE id = auth.uid() AND role IN ('manager', 'admin'))
    );


-- 4. USERS TABLE HARDENING
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_name='users' AND column_name='is_active') THEN
        ALTER TABLE public.users ADD COLUMN is_active BOOLEAN DEFAULT true;
    END IF;
END $$;


-- 5. ROW ASSIGNMENTS TABLE (HeatMap Integration)
CREATE TABLE IF NOT EXISTS public.row_assignments (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    row_number INTEGER NOT NULL,
    status TEXT DEFAULT 'active' CHECK (status IN ('active', 'completed')),
    assigned_at TIMESTAMPTZ DEFAULT now(),
    UNIQUE(orchard_id, row_number)
);

-- Enable RLS
ALTER TABLE public.row_assignments ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS "Anyone read row assignments" ON public.row_assignments;
CREATE POLICY "Anyone read row assignments" ON public.row_assignments
    FOR SELECT TO authenticated USING (true);

DROP POLICY IF EXISTS "Managers manage row assignments" ON public.row_assignments;
CREATE POLICY "Managers manage row assignments" ON public.row_assignments
    FOR ALL USING (
        EXISTS (
            SELECT 1 FROM public.users 
            WHERE id = auth.uid() AND role IN ('manager', 'admin')
        )
    );


-- 6. BUCKET RECORDS HARDENING
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_name='bucket_records' AND column_name='row_number') THEN
        ALTER TABLE public.bucket_records ADD COLUMN row_number INTEGER;
    END IF;
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_name='bucket_records' AND column_name='quality_grade') THEN
        ALTER TABLE public.bucket_records ADD COLUMN quality_grade TEXT;
    END IF;
END $$;

-- 7. PICKERS ROLE HARDENING
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_name='pickers' AND column_name='role') THEN
        ALTER TABLE public.pickers ADD COLUMN role TEXT DEFAULT 'picker';
    END IF;
END $$;

-- Sync roles for existing pickers from users table
UPDATE public.pickers p
SET role = u.role
FROM public.users u
WHERE p.id = u.id AND p.role = 'picker';


-- 7. PERFORMANCE VIEW (Today's stats)
DROP VIEW IF EXISTS public.pickers_performance_today;
CREATE OR REPLACE VIEW public.pickers_performance_today AS
  SELECT 
    p.id as picker_id,
    p.name as picker_name, -- Added for easier Dashboard integration
    p.orchard_id,
    p.team_leader_id,
    COUNT(b.id) as total_buckets,
    CASE 
        WHEN MIN(b.scanned_at) IS NULL THEN 0
        ELSE EXTRACT(EPOCH FROM (NOW() - MIN(b.scanned_at))) / 3600 + 0.5 
    END as hours_worked
  FROM public.pickers p
  LEFT JOIN public.bucket_records b ON p.id = b.picker_id AND b.scanned_at >= CURRENT_DATE
  GROUP BY p.id, p.name, p.orchard_id, p.team_leader_id;

GRANT SELECT ON public.pickers_performance_today TO authenticated;


-- 8. SEED DEFAULT DATA
-- Ensure default orchard has settings
INSERT INTO public.harvest_settings (orchard_id, min_wage_rate, piece_rate, min_buckets_per_hour, target_tons)
SELECT id, 23.50, 6.50, 3.6, 40.0
FROM public.orchards
WHERE id = 'a0000000-0000-0000-0000-000000000001'
ON CONFLICT (orchard_id) DO NOTHING;

-- Verification
SELECT 'Repairs Applied Successfully' as status;
