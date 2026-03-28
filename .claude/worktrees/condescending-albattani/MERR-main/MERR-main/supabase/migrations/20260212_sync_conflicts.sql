-- =============================================
-- SYNC CONFLICTS TABLE
-- =============================================
-- Version: 1.0
-- Created: 2026-02-12
-- Purpose: Audit trail for offline sync conflicts
-- =============================================
CREATE TABLE IF NOT EXISTS public.sync_conflicts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES auth.users(id),
    table_name TEXT NOT NULL,
    record_id UUID,
    local_updated_at TIMESTAMPTZ,
    server_updated_at TIMESTAMPTZ,
    local_values JSONB,
    server_values JSONB,
    resolution TEXT CHECK (
        resolution IN (
            'keep_local',
            'keep_server',
            'merged',
            'auto_resolved'
        )
    ),
    resolved_at TIMESTAMPTZ,
    resolved_by UUID REFERENCES auth.users(id),
    created_at TIMESTAMPTZ DEFAULT now()
);
-- Indexes
CREATE INDEX IF NOT EXISTS idx_sync_conflicts_user ON public.sync_conflicts(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_sync_conflicts_table ON public.sync_conflicts(table_name, record_id);
CREATE INDEX IF NOT EXISTS idx_sync_conflicts_unresolved ON public.sync_conflicts(resolution)
WHERE resolution IS NULL;
-- Enable RLS
ALTER TABLE public.sync_conflicts ENABLE ROW LEVEL SECURITY;
-- Only managers can view all conflicts
CREATE POLICY "managers_view_sync_conflicts" ON public.sync_conflicts FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role = 'manager'
        )
    );
-- Users can view their own conflicts
CREATE POLICY "users_view_own_conflicts" ON public.sync_conflicts FOR
SELECT USING (auth.uid() = user_id);
-- All authenticated users can insert conflicts
CREATE POLICY "insert_sync_conflicts" ON public.sync_conflicts FOR
INSERT WITH CHECK (auth.uid() = user_id);
COMMENT ON TABLE public.sync_conflicts IS 'Audit trail for offline sync conflicts (last-write-wins with logging)';