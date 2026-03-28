-- =============================================
-- AUDIT LOGS TABLE - Security audit trail
-- =============================================
-- Run this migration in your Supabase SQL Editor

-- Create audit_logs table
CREATE TABLE IF NOT EXISTS audit_logs (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    event_type VARCHAR(50) NOT NULL,
    severity VARCHAR(20) DEFAULT 'info',
    user_id UUID REFERENCES auth.users(id),
    user_email VARCHAR(255),
    orchard_id UUID,
    entity_type VARCHAR(50),
    entity_id VARCHAR(100),
    action TEXT NOT NULL,
    details JSONB,
    ip_address VARCHAR(45),
    user_agent TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Create indexes for common queries
CREATE INDEX IF NOT EXISTS idx_audit_logs_event_type ON audit_logs(event_type);
CREATE INDEX IF NOT EXISTS idx_audit_logs_user_id ON audit_logs(user_id);
CREATE INDEX IF NOT EXISTS idx_audit_logs_orchard_id ON audit_logs(orchard_id);
CREATE INDEX IF NOT EXISTS idx_audit_logs_created_at ON audit_logs(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_audit_logs_severity ON audit_logs(severity) WHERE severity IN ('error', 'critical');

-- Create composite index for filtering by type and date
CREATE INDEX IF NOT EXISTS idx_audit_logs_type_date ON audit_logs(event_type, created_at DESC);

-- =============================================
-- ROW LEVEL SECURITY (RLS)
-- =============================================

-- Enable RLS
ALTER TABLE audit_logs ENABLE ROW LEVEL SECURITY;

-- Policy: Managers can view all audit logs for their orchard
CREATE POLICY "Managers can view orchard audit logs"
    ON audit_logs FOR SELECT
    TO authenticated
    USING (
        orchard_id IN (
            SELECT orchard_id FROM users WHERE id = auth.uid() AND role = 'manager'
        )
    );

-- Policy: Allow authenticated users to insert their own audit logs
CREATE POLICY "Users can insert audit logs"
    ON audit_logs FOR INSERT
    TO authenticated
    WITH CHECK (
        user_id = auth.uid() OR user_id IS NULL
    );

-- Policy: Only admins can delete/update audit logs (generally should not happen)
CREATE POLICY "Admins only can modify audit logs"
    ON audit_logs FOR ALL
    TO authenticated
    USING (
        EXISTS (
            SELECT 1 FROM users WHERE id = auth.uid() AND role = 'admin'
        )
    );

-- =============================================
-- COMMENTS
-- =============================================
COMMENT ON TABLE audit_logs IS 'Security audit trail for all critical application events';
COMMENT ON COLUMN audit_logs.event_type IS 'Type of event: auth.login, picker.created, etc.';
COMMENT ON COLUMN audit_logs.severity IS 'Event severity: info, warning, error, critical';
COMMENT ON COLUMN audit_logs.entity_type IS 'Type of entity affected: picker, user, bin, etc.';
COMMENT ON COLUMN audit_logs.entity_id IS 'ID of the affected entity';
COMMENT ON COLUMN audit_logs.details IS 'Additional event details as JSON';

-- =============================================
-- RETENTION POLICY (Optional - run periodically)
-- =============================================
-- Delete audit logs older than 90 days (info only, keep warnings/errors longer)
-- CREATE OR REPLACE FUNCTION cleanup_old_audit_logs() RETURNS void AS $$
-- BEGIN
--     DELETE FROM audit_logs 
--     WHERE severity = 'info' 
--     AND created_at < NOW() - INTERVAL '90 days';
--     
--     DELETE FROM audit_logs 
--     WHERE severity = 'warning' 
--     AND created_at < NOW() - INTERVAL '180 days';
-- END;
-- $$ LANGUAGE plpgsql;
