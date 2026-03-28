-- =============================================
-- MESSAGING_SETUP.SQL
-- Ensures all tables for Unified Messaging are present
-- =============================================

-- 1. CONVERSATIONS (DMs and Groups)
CREATE TABLE IF NOT EXISTS public.conversations (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    type TEXT NOT NULL CHECK (type IN ('direct', 'group', 'broadcast')),
    name TEXT,
    participant_ids TEXT[] NOT NULL DEFAULT '{}', -- Array of user UUIDs as strings
    created_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- 2. CHAT MESSAGES
CREATE TABLE IF NOT EXISTS public.chat_messages (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID NOT NULL REFERENCES public.conversations(id) ON DELETE CASCADE,
    sender_id UUID NOT NULL REFERENCES public.users(id),
    content TEXT NOT NULL,
    read_by TEXT[] DEFAULT '{}',
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- 3. BROADCASTS (Orchard-wide alerts)
CREATE TABLE IF NOT EXISTS public.broadcasts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    sender_id UUID NOT NULL REFERENCES public.users(id),
    title TEXT NOT NULL,
    content TEXT NOT NULL,
    priority TEXT DEFAULT 'normal',
    target_roles TEXT[] DEFAULT '{team_leader, runner}',
    acknowledged_by TEXT[] DEFAULT '{}',
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Indices for performance
CREATE INDEX IF NOT EXISTS idx_conversations_participants ON public.conversations USING GIN (participant_ids);
CREATE INDEX IF NOT EXISTS idx_chat_messages_conversation ON public.chat_messages(conversation_id);
CREATE INDEX IF NOT EXISTS idx_broadcasts_orchard ON public.broadcasts(orchard_id);

-- RLS
ALTER TABLE public.conversations ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.chat_messages ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.broadcasts ENABLE ROW LEVEL SECURITY;

-- Policies
DROP POLICY IF EXISTS "Anyone in conversation can view it" ON public.conversations;
CREATE POLICY "Anyone in conversation can view it" ON public.conversations
    FOR SELECT USING (auth.uid()::text = ANY(participant_ids));

DROP POLICY IF EXISTS "Anyone in conversation can view messages" ON public.chat_messages;
CREATE POLICY "Anyone in conversation can view messages" ON public.chat_messages
    FOR SELECT USING (
        EXISTS (
            SELECT 1 FROM public.conversations c 
            WHERE c.id = conversation_id AND auth.uid()::text = ANY(c.participant_ids)
        )
    );

DROP POLICY IF EXISTS "Managers can send broadcasts" ON public.broadcasts;
CREATE POLICY "Managers can send broadcasts" ON public.broadcasts
    FOR INSERT WITH CHECK (
        EXISTS (SELECT 1 FROM public.users WHERE id = auth.uid() AND role = 'manager')
    );

DROP POLICY IF EXISTS "Users can view relevant broadcasts" ON public.broadcasts;
CREATE POLICY "Users can view relevant broadcasts" ON public.broadcasts
    FOR SELECT USING (
        orchard_id = (SELECT orchard_id FROM public.users WHERE id = auth.uid())
    );

-- Realtime
DO $$
BEGIN
    ALTER PUBLICATION supabase_realtime ADD TABLE public.chat_messages;
    ALTER PUBLICATION supabase_realtime ADD TABLE public.conversations;
    ALTER PUBLICATION supabase_realtime ADD TABLE public.broadcasts;
EXCEPTION WHEN OTHERS THEN NULL;
END $$;
