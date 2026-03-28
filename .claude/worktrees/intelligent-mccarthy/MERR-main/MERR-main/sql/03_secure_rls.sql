-- SECURE RLS POLICIES MIGRATION
-- Enforces orchard_id isolation for data security

-- Helper: Function to get current user's orchard_id
CREATE OR REPLACE FUNCTION get_auth_orchard_id()
RETURNS UUID AS $$
BEGIN
  RETURN (SELECT orchard_id FROM public.users WHERE id = auth.uid());
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- 1. SCANNED_STICKERS
DROP POLICY IF EXISTS "Users can view scanned stickers" ON scanned_stickers;
DROP POLICY IF EXISTS "Users can insert scanned stickers" ON scanned_stickers;

CREATE POLICY "View scanned stickers from same orchard" ON scanned_stickers
    FOR SELECT USING (orchard_id = get_auth_orchard_id());

CREATE POLICY "Insert scanned stickers for same orchard" ON scanned_stickers
    FOR INSERT WITH CHECK (orchard_id = get_auth_orchard_id());

-- 2. DAY_SETUPS
DROP POLICY IF EXISTS "All members can read active day_setups" ON day_setups;
CREATE POLICY "All members can include day_setups from same orchard" ON day_setups
    FOR SELECT USING (orchard_id = get_auth_orchard_id());

-- 3. ROW_ASSIGNMENTS (Assuming table exists, commonly used)
ALTER TABLE IF EXISTS row_assignments ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "View row assignments" ON row_assignments;
CREATE POLICY "View row assignments from same orchard" ON row_assignments
    FOR SELECT USING (
        EXISTS (
            SELECT 1 FROM day_setups 
            WHERE day_setups.id = row_assignments.day_setup_id 
            AND day_setups.orchard_id = get_auth_orchard_id()
        )
    );

-- 4. MESSAGING (Cleanup old table if it exists)
-- If we are using conversations/chat_messages now, we might want to drop 'messages' 
-- OR strictly enforce policies on 'messages' if legacy code uses it.
-- For now, let's just ensure 'messages' is also secured if it stays.
DO $$ 
BEGIN 
    IF EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = 'messages') THEN
        DROP POLICY IF EXISTS "Users can read all messages" ON messages;
        CREATE POLICY "Users can read messages from same orchard" ON messages
            FOR SELECT USING (orchard_id = get_auth_orchard_id());
    END IF;
END $$;
