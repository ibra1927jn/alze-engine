-- =============================================
-- HARVESTPRO NZ - FULL DATABASE RESET & REBUILD V1
-- Created: 2026-02-07
-- WARNING: THIS SCRIPT WILL DELETE ALL DATA IN THE DATABASE.
-- Objective: Wipes the database clean and applies the consolidated schema v1.
-- =============================================

-- =============================================
-- PART 1: THE PURGE (DROP EVERYTHING)
-- =============================================

-- Drop Tables (Child tables first to satisfy Foreign Keys)
DROP TABLE IF EXISTS public.chat_messages CASCADE;
DROP TABLE IF EXISTS public.conversations CASCADE;
DROP TABLE IF EXISTS public.quality_inspections CASCADE;
DROP TABLE IF EXISTS public.bucket_records CASCADE;
DROP TABLE IF EXISTS public.bins CASCADE;
DROP TABLE IF EXISTS public.day_setups CASCADE;
DROP TABLE IF EXISTS public.pickers CASCADE;
DROP TABLE IF EXISTS public.users CASCADE;
DROP TABLE IF EXISTS public.orchards CASCADE;
DROP TABLE IF EXISTS public.chat_groups CASCADE; -- Legacy table
DROP TABLE IF EXISTS public.messages CASCADE; -- Legacy table

-- =============================================
-- PART 2: THE REBUILD (APPLY CONSOLIDATED SCHEMA)
-- =============================================

-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- HOTFIX: Ensure columns exist if table was created by an old script
DO $$
BEGIN
    ALTER TABLE public.orchards ADD COLUMN IF NOT EXISTS code TEXT UNIQUE;
    ALTER TABLE public.orchards ADD COLUMN IF NOT EXISTS total_blocks INTEGER DEFAULT 0;
EXCEPTION
    WHEN duplicate_column THEN RAISE NOTICE 'Column already exists, skipping.';
END $$;

-- 1.1 ORCHARDS (Base Organization Unit)
CREATE TABLE IF NOT EXISTS public.orchards (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    code TEXT UNIQUE,
    name TEXT NOT NULL,
    location TEXT,
    total_blocks INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- Insert Default Orchard if not exists
INSERT INTO public.orchards (id, code, name, location)
SELECT 
    'a0000000-0000-0000-0000-000000000001'::UUID,
    'DEMO001',
    'Default Orchard',
    'Central Otago, NZ'
WHERE NOT EXISTS (SELECT 1 FROM public.orchards LIMIT 1);


-- 1.2 USERS (Extends auth.users)
CREATE TABLE IF NOT EXISTS public.users (
    id UUID PRIMARY KEY REFERENCES auth.users(id) ON DELETE CASCADE,
    email TEXT,
    full_name TEXT,
    role TEXT DEFAULT 'team_leader' CHECK (role IN ('manager', 'team_leader', 'runner', 'qc_inspector')),
    orchard_id UUID REFERENCES public.orchards(id),
    team_id UUID,
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- 1.3 PICKERS (Seasonal Workers)
CREATE TABLE IF NOT EXISTS public.pickers (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    picker_id TEXT UNIQUE NOT NULL, -- Badge ID / Employee ID
    name TEXT NOT NULL,
    orchard_id UUID REFERENCES public.orchards(id),
    team_leader_id UUID REFERENCES public.users(id),
    safety_verified BOOLEAN DEFAULT false, -- Renamed from onboarded
    total_buckets_today INTEGER DEFAULT 0,
    current_row INTEGER DEFAULT 0, -- Added for map visualization
    status TEXT DEFAULT 'active',
    created_at TIMESTAMPTZ DEFAULT now()
);

-- 1.4 DAY SETUPS (Daily Config)
CREATE TABLE IF NOT EXISTS public.day_setups (
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

-- 1.5 BUCKET RECORDS (Core Transaction)
CREATE TABLE IF NOT EXISTS public.bucket_records (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    picker_id UUID REFERENCES public.pickers(id), -- Or users(id) if runners scan too? Usually pickers.
    bin_id UUID, -- Optional linkage to a bin
    scanned_by UUID REFERENCES public.users(id), -- The runner who scanned
    scanned_at TIMESTAMPTZ DEFAULT now(), -- Standardized timestamp
    coords JSONB, -- {lat, lng} for heatmap
    created_at TIMESTAMPTZ DEFAULT now() -- Audit
);

-- 1.6 BINS (Logistics)
CREATE TABLE IF NOT EXISTS public.bins (
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

-- 1.7 QUALITY INSPECTIONS
CREATE TABLE IF NOT EXISTS public.quality_inspections (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    bucket_id UUID REFERENCES public.bucket_records(id),
    picker_id UUID REFERENCES public.pickers(id), -- Or users(id)
    inspector_id UUID REFERENCES public.users(id),
    quality_grade TEXT CHECK (quality_grade IN ('good', 'warning', 'bad', 'A', 'B', 'C', 'reject')),
    notes TEXT,
    photo_url TEXT,
    coords JSONB,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- =============================================
-- 2. MESSAGING SYSTEM (Simple Schema)
-- =============================================

-- 2.1 CONVERSATIONS
CREATE TABLE IF NOT EXISTS public.conversations (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    type TEXT NOT NULL CHECK (type IN ('direct', 'group', 'broadcast')),
    name TEXT,
    participant_ids TEXT[] NOT NULL DEFAULT '{}', -- Array of user UUIDs
    created_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 2.2 CHAT MESSAGES
CREATE TABLE IF NOT EXISTS public.chat_messages (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID NOT NULL REFERENCES public.conversations(id) ON DELETE CASCADE,
    sender_id UUID NOT NULL REFERENCES public.users(id),
    content TEXT NOT NULL,
    read_by TEXT[] DEFAULT '{}',
    created_at TIMESTAMPTZ DEFAULT NOW()
);


-- =============================================
-- 3. SECURITY & RLS (HARDENING)
-- =============================================

-- Enable RLS on ALL tables
ALTER TABLE public.orchards ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.users ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.pickers ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.day_setups ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bucket_records ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bins ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.quality_inspections ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.conversations ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.chat_messages ENABLE ROW LEVEL SECURITY;

-- 3.1 HELPER FUNCTIONS (SECURITY DEFINER to prevent recursion)
-- Critical: This function runs as owner, bypassing RLS to check permissions safely
CREATE OR REPLACE FUNCTION public.get_my_orchard_id()
RETURNS UUID
LANGUAGE sql
SECURITY DEFINER
SET search_path = public
STABLE
AS $$
  SELECT orchard_id FROM public.users WHERE id = auth.uid() LIMIT 1;
$$;

CREATE OR REPLACE FUNCTION public.is_manager_or_leader()
RETURNS BOOLEAN
LANGUAGE sql
SECURITY DEFINER
SET search_path = public
STABLE
AS $$
  SELECT EXISTS (
    SELECT 1 FROM public.users 
    WHERE id = auth.uid() AND role IN ('manager', 'team_leader')
  );
$$;

-- 3.2 ORCHARDS POLICIES
-- Allow authenticated users to read orchards (e.g. for selection or context)
DROP POLICY IF EXISTS "Authenticated read orchards" ON public.orchards;
CREATE POLICY "Authenticated read orchards" ON public.orchards
    FOR SELECT TO authenticated USING (true);

-- 3.3 USERS POLICIES
DROP POLICY IF EXISTS "Users interactions" ON public.users; -- Cleanup old policy

-- Read Self
DROP POLICY IF EXISTS "Read self" ON public.users;
CREATE POLICY "Read self" ON public.users
    FOR SELECT USING (auth.uid() = id);

-- Read Co-workers (same orchard) - Uses SECURITY DEFINER function
DROP POLICY IF EXISTS "Read orchard members" ON public.users;
CREATE POLICY "Read orchard members" ON public.users
    FOR SELECT USING (orchard_id = get_my_orchard_id());

-- Update Self
DROP POLICY IF EXISTS "Update self" ON public.users;
CREATE POLICY "Update self" ON public.users
    FOR UPDATE USING (auth.uid() = id);

-- Insert Self (Registration)
DROP POLICY IF EXISTS "Insert self" ON public.users;
CREATE POLICY "Insert self" ON public.users
    FOR INSERT WITH CHECK (auth.uid() = id);

-- 3.4 PICKERS POLICIES
-- Managers/Leaders manage, Runners/Others might read
DROP POLICY IF EXISTS "Manage pickers" ON public.pickers;
CREATE POLICY "Manage pickers" ON public.pickers
    FOR ALL USING (is_manager_or_leader());

DROP POLICY IF EXISTS "Read pickers" ON public.pickers;
CREATE POLICY "Read pickers" ON public.pickers
    FOR SELECT USING (orchard_id = get_my_orchard_id());

-- 3.5 BUCKET RECORDS POLICIES
-- Runners INSERT, Managers/Leaders SELECT ALL, Runners SELECT OWN (or session)
DROP POLICY IF EXISTS "Runners insert records" ON public.bucket_records;
CREATE POLICY "Runners insert records" ON public.bucket_records
    FOR INSERT WITH CHECK (auth.uid() = scanned_by);

DROP POLICY IF EXISTS "View orchard records" ON public.bucket_records;
CREATE POLICY "View orchard records" ON public.bucket_records
    FOR SELECT USING (
        orchard_id = get_my_orchard_id() 
        AND (is_manager_or_leader() OR scanned_by = auth.uid())
    );

-- 3.6 MESSAGING POLICIES
-- Conversations: View if participant
DROP POLICY IF EXISTS "View conversations" ON public.conversations;
CREATE POLICY "View conversations" ON public.conversations
    FOR SELECT USING (auth.uid()::text = ANY(participant_ids));

DROP POLICY IF EXISTS "Create conversations" ON public.conversations;
CREATE POLICY "Create conversations" ON public.conversations
    FOR INSERT WITH CHECK (auth.uid()::text = ANY(participant_ids));

DROP POLICY IF EXISTS "Update conversations" ON public.conversations;
CREATE POLICY "Update conversations" ON public.conversations
    FOR UPDATE USING (auth.uid()::text = ANY(participant_ids));

-- Messages: View if in conversation
DROP POLICY IF EXISTS "View messages" ON public.chat_messages;
CREATE POLICY "View messages" ON public.chat_messages
    FOR SELECT USING (
        EXISTS (
            SELECT 1 FROM public.conversations c
            WHERE c.id = conversation_id
            AND auth.uid()::text = ANY(c.participant_ids)
        )
    );

DROP POLICY IF EXISTS "Send messages" ON public.chat_messages;
CREATE POLICY "Send messages" ON public.chat_messages
    FOR INSERT WITH CHECK (
        auth.uid() = sender_id
        AND EXISTS (
            SELECT 1 FROM public.conversations c
            WHERE c.id = conversation_id
            AND auth.uid()::text = ANY(c.participant_ids)
        )
    );

-- =============================================
-- 4. PERFORMANCE OPTIMIZATION (INDICES)
-- =============================================

-- Metadata & Foreign Keys
CREATE INDEX IF NOT EXISTS idx_users_orchard ON public.users(orchard_id);
CREATE INDEX IF NOT EXISTS idx_pickers_orchard ON public.pickers(orchard_id);
CREATE INDEX IF NOT EXISTS idx_pickers_code ON public.pickers(picker_id);

-- Operational Data
CREATE INDEX IF NOT EXISTS idx_bucket_records_picker ON public.bucket_records(picker_id);
CREATE INDEX IF NOT EXISTS idx_bucket_records_scanned_by ON public.bucket_records(scanned_by);
CREATE INDEX IF NOT EXISTS idx_bucket_records_created ON public.bucket_records(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_bucket_records_orchard ON public.bucket_records(orchard_id);

-- Messaging
CREATE INDEX IF NOT EXISTS idx_conversations_participants ON public.conversations USING gin(participant_ids);
CREATE INDEX IF NOT EXISTS idx_chat_messages_conversation ON public.chat_messages(conversation_id);
CREATE INDEX IF NOT EXISTS idx_chat_messages_created ON public.chat_messages(created_at DESC);

-- =============================================
-- 5. REALTIME SETUP
-- =============================================
DO $$
BEGIN
    ALTER PUBLICATION supabase_realtime ADD TABLE public.chat_messages;
    ALTER PUBLICATION supabase_realtime ADD TABLE public.conversations;
    ALTER PUBLICATION supabase_realtime ADD TABLE public.bucket_records;
    ALTER PUBLICATION supabase_realtime ADD TABLE public.users;
EXCEPTION WHEN OTHERS THEN NULL;
END $$;

-- 6. VERIFICATION
SELECT 'FULL RESET COMPLETE. Database is now at v1 schema state.' as result;
SELECT * FROM public.orchards;
