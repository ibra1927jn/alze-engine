-- =============================================
-- HARVESTPRO NZ - FINAL HARD RESET SCRIPT
-- =============================================
-- 1. THE PURGE (Drop All Existing Tables)
DROP TABLE IF EXISTS public.chat_messages CASCADE;
DROP TABLE IF EXISTS public.conversations CASCADE;
DROP TABLE IF EXISTS public.quality_inspections CASCADE;
DROP TABLE IF EXISTS public.bucket_records CASCADE;
DROP TABLE IF EXISTS public.bins CASCADE;
DROP TABLE IF EXISTS public.day_setups CASCADE;
DROP TABLE IF EXISTS public.pickers CASCADE;
DROP TABLE IF EXISTS public.users CASCADE;
DROP TABLE IF EXISTS public.orchards CASCADE;
DROP TABLE IF EXISTS public.chat_groups CASCADE;
DROP TABLE IF EXISTS public.messages CASCADE;

-- 2. THE REBUILD (Base Tables)
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE public.orchards (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    code TEXT UNIQUE,
    name TEXT NOT NULL,
    location TEXT,
    total_blocks INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- Seed Default Orchard
INSERT INTO public.orchards (id, code, name, location) VALUES ('a0000000-0000-0000-0000-000000000001'::UUID, 'DEMO001', 'Demo Orchard', 'Central Otago, NZ');

CREATE TABLE public.users (
    id UUID PRIMARY KEY REFERENCES auth.users(id) ON DELETE CASCADE,
    email TEXT,
    full_name TEXT,
    role TEXT DEFAULT 'team_leader' CHECK (role IN ('manager', 'team_leader', 'runner', 'qc_inspector')),
    orchard_id UUID REFERENCES public.orchards(id),
    team_id UUID,
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE public.pickers (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    picker_id TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    orchard_id UUID REFERENCES public.orchards(id),
    team_leader_id UUID REFERENCES public.users(id),
    safety_verified BOOLEAN DEFAULT false,
    total_buckets_today INTEGER DEFAULT 0,
    current_row INTEGER DEFAULT 0,
    status TEXT DEFAULT 'active',
    created_at TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE public.day_setups (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    date DATE DEFAULT CURRENT_DATE,
    variety TEXT,
    target_tons DECIMAL(10,2),
    piece_rate DECIMAL(10,2) DEFAULT 6.50,
    min_wage_rate DECIMAL(10,2) DEFAULT 23.50,
    start_time TIME,
    created_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE public.bucket_records (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    picker_id UUID REFERENCES public.pickers(id),
    bin_id UUID,
    scanned_by UUID REFERENCES public.users(id),
    scanned_at TIMESTAMPTZ DEFAULT now(),
    coords JSONB,
    created_at TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE public.bins (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    bin_code TEXT,
    status TEXT DEFAULT 'empty' CHECK (status IN ('empty', 'partial', 'full', 'collected')),
    variety TEXT,
    location JSONB,
    movement_history JSONB[] DEFAULT '{}',
    filled_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE public.quality_inspections (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    bucket_id UUID REFERENCES public.bucket_records(id),
    picker_id UUID REFERENCES public.pickers(id),
    inspector_id UUID REFERENCES public.users(id),
    quality_grade TEXT CHECK (quality_grade IN ('good', 'warning', 'bad', 'A', 'B', 'C', 'reject')),
    notes TEXT,
    photo_url TEXT,
    coords JSONB,
    created_at TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE public.conversations (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    type TEXT NOT NULL CHECK (type IN ('direct', 'group', 'broadcast')),
    name TEXT,
    participant_ids TEXT[] NOT NULL DEFAULT '{}',
    created_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE public.chat_messages (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID NOT NULL REFERENCES public.conversations(id) ON DELETE CASCADE,
    sender_id UUID NOT NULL REFERENCES public.users(id),
    content TEXT NOT NULL,
    read_by TEXT[] DEFAULT '{}',
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- 3. SECURITY & RLS
ALTER TABLE public.orchards ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.users ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.pickers ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.day_setups ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bucket_records ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bins ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.quality_inspections ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.conversations ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.chat_messages ENABLE ROW LEVEL SECURITY;

-- Helper Functions
CREATE OR REPLACE FUNCTION public.get_my_orchard_id() RETURNS UUID LANGUAGE sql SECURITY DEFINER SET search_path = public STABLE AS $$ SELECT orchard_id FROM public.users WHERE id = auth.uid() LIMIT 1; $$;

CREATE OR REPLACE FUNCTION public.is_manager_or_leader() RETURNS BOOLEAN LANGUAGE sql SECURITY DEFINER SET search_path = public STABLE AS $$ SELECT EXISTS (SELECT 1 FROM public.users WHERE id = auth.uid() AND role IN ('manager', 'team_leader')); $$;

-- Policies (Idempotent)
-- ORCHARDS
DROP POLICY IF EXISTS "Authenticated read orchards" ON public.orchards;
CREATE POLICY "Authenticated read orchards" ON public.orchards FOR SELECT TO authenticated USING (true);

-- USERS
DROP POLICY IF EXISTS "Read self" ON public.users;
CREATE POLICY "Read self" ON public.users FOR SELECT USING (auth.uid() = id);

DROP POLICY IF EXISTS "Read orchard members" ON public.users;
CREATE POLICY "Read orchard members" ON public.users FOR SELECT USING (orchard_id = get_my_orchard_id());

DROP POLICY IF EXISTS "Update self" ON public.users;
CREATE POLICY "Update self" ON public.users FOR UPDATE USING (auth.uid() = id);

DROP POLICY IF EXISTS "Insert self" ON public.users;
CREATE POLICY "Insert self" ON public.users FOR INSERT WITH CHECK (auth.uid() = id);

-- PICKERS
DROP POLICY IF EXISTS "Manage pickers" ON public.pickers;
CREATE POLICY "Manage pickers" ON public.pickers FOR ALL USING (is_manager_or_leader());

DROP POLICY IF EXISTS "Read pickers" ON public.pickers;
CREATE POLICY "Read pickers" ON public.pickers FOR SELECT USING (orchard_id = get_my_orchard_id());

-- BUCKET RECORDS
DROP POLICY IF EXISTS "Runners insert records" ON public.bucket_records;
CREATE POLICY "Runners insert records" ON public.bucket_records FOR INSERT WITH CHECK (auth.uid() = scanned_by);

DROP POLICY IF EXISTS "View orchard records" ON public.bucket_records;
CREATE POLICY "View orchard records" ON public.bucket_records FOR SELECT USING ( orchard_id = get_my_orchard_id() AND (is_manager_or_leader() OR scanned_by = auth.uid()) );

-- MESSAGING
DROP POLICY IF EXISTS "View conversations" ON public.conversations;
CREATE POLICY "View conversations" ON public.conversations FOR SELECT USING (auth.uid()::text = ANY(participant_ids));

DROP POLICY IF EXISTS "Create conversations" ON public.conversations;
CREATE POLICY "Create conversations" ON public.conversations FOR INSERT WITH CHECK (auth.uid()::text = ANY(participant_ids));

DROP POLICY IF EXISTS "Update conversations" ON public.conversations;
CREATE POLICY "Update conversations" ON public.conversations FOR UPDATE USING (auth.uid()::text = ANY(participant_ids));

DROP POLICY IF EXISTS "View messages" ON public.chat_messages;
CREATE POLICY "View messages" ON public.chat_messages FOR SELECT USING ( EXISTS (SELECT 1 FROM public.conversations c WHERE c.id = conversation_id AND auth.uid()::text = ANY(c.participant_ids)) );

DROP POLICY IF EXISTS "Send messages" ON public.chat_messages;
CREATE POLICY "Send messages" ON public.chat_messages FOR INSERT WITH CHECK ( auth.uid() = sender_id AND EXISTS (SELECT 1 FROM public.conversations c WHERE c.id = conversation_id AND auth.uid()::text = ANY(c.participant_ids)) );

-- 4. INDICES
CREATE INDEX IF NOT EXISTS idx_users_orchard ON public.users(orchard_id);
CREATE INDEX IF NOT EXISTS idx_pickers_orchard ON public.pickers(orchard_id);
CREATE INDEX IF NOT EXISTS idx_pickers_code ON public.pickers(picker_id);
CREATE INDEX IF NOT EXISTS idx_bucket_records_picker ON public.bucket_records(picker_id);
CREATE INDEX IF NOT EXISTS idx_bucket_records_scanned_by ON public.bucket_records(scanned_by);
CREATE INDEX IF NOT EXISTS idx_bucket_records_created ON public.bucket_records(created_at DESC);

-- 5. REALTIME
DO $$ BEGIN ALTER PUBLICATION supabase_realtime ADD TABLE public.chat_messages; ALTER PUBLICATION supabase_realtime ADD TABLE public.bucket_records; EXCEPTION WHEN OTHERS THEN NULL; END $$;

SELECT 'SUCCESS: Database Reset & Seeded' as status;
