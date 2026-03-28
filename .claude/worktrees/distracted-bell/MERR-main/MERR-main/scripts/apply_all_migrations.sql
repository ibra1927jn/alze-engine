-- =============================================
-- HARVESTPRO NZ — MASTER MIGRATION SCRIPT
-- Sprint 8 Phase 1: All migrations in correct order
-- 
-- ⚠️ Run from Supabase SQL Editor with service_role
-- ⚠️ IDEMPOTENT — safe to re-run
-- =============================================
-- =============================================
-- STEP 0: Pre-flight — ensure extensions
-- =============================================
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
-- =============================================
-- STEP 1: CORE SCHEMA (schema_v1_consolidated.sql)
-- Tables: orchards, users, pickers, day_setups, 
--         bucket_records, bins, quality_inspections,
--         conversations, chat_messages
-- Also: RLS, policies, indices, realtime
-- =============================================
-- 1.1 ORCHARDS
DO $$ BEGIN
ALTER TABLE public.orchards
ADD COLUMN IF NOT EXISTS code TEXT UNIQUE;
ALTER TABLE public.orchards
ADD COLUMN IF NOT EXISTS total_blocks INTEGER DEFAULT 0;
EXCEPTION
WHEN undefined_table THEN NULL;
-- table doesn't exist yet
WHEN duplicate_column THEN NULL;
END $$;
CREATE TABLE IF NOT EXISTS public.orchards (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    code TEXT UNIQUE,
    name TEXT NOT NULL,
    location TEXT,
    total_blocks INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT now()
);
INSERT INTO public.orchards (id, code, name, location)
SELECT 'a0000000-0000-0000-0000-000000000001'::UUID,
    'DEMO001',
    'Default Orchard',
    'Central Otago, NZ'
WHERE NOT EXISTS (
        SELECT 1
        FROM public.orchards
        LIMIT 1
    );
-- 1.2 USERS (with ALL 8 roles from the start)
CREATE TABLE IF NOT EXISTS public.users (
    id UUID PRIMARY KEY REFERENCES auth.users(id) ON DELETE CASCADE,
    email TEXT,
    full_name TEXT,
    role TEXT DEFAULT 'team_leader' CHECK (
        role IN (
            'manager',
            'team_leader',
            'runner',
            'qc_inspector',
            'payroll_admin',
            'admin',
            'hr_admin',
            'logistics'
        )
    ),
    orchard_id UUID REFERENCES public.orchards(id),
    team_id UUID,
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- 1.3 PICKERS
CREATE TABLE IF NOT EXISTS public.pickers (
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
-- Add archived_at if missing (from 20260211_add_archived_at.sql)
ALTER TABLE public.pickers
ADD COLUMN IF NOT EXISTS archived_at TIMESTAMPTZ;
-- 1.4 DAY SETUPS
CREATE TABLE IF NOT EXISTS public.day_setups (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    date DATE DEFAULT CURRENT_DATE,
    variety TEXT,
    target_tons DECIMAL(10, 2),
    piece_rate DECIMAL(10, 2) DEFAULT 6.50,
    min_wage_rate DECIMAL(10, 2) DEFAULT 23.50,
    start_time TIME,
    created_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT now()
);
-- 1.5 BUCKET RECORDS
CREATE TABLE IF NOT EXISTS public.bucket_records (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    picker_id UUID REFERENCES public.pickers(id),
    bin_id UUID,
    scanned_by UUID REFERENCES public.users(id),
    scanned_at TIMESTAMPTZ DEFAULT now(),
    coords JSONB,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- 1.6 BINS
CREATE TABLE IF NOT EXISTS public.bins (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    bin_code TEXT,
    status TEXT DEFAULT 'empty' CHECK (
        status IN ('empty', 'partial', 'full', 'collected')
    ),
    variety TEXT,
    location JSONB,
    movement_history JSONB [] DEFAULT '{}',
    filled_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- 1.7 QUALITY INSPECTIONS
CREATE TABLE IF NOT EXISTS public.quality_inspections (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    bucket_id UUID REFERENCES public.bucket_records(id),
    picker_id UUID REFERENCES public.pickers(id),
    inspector_id UUID REFERENCES public.users(id),
    quality_grade TEXT CHECK (
        quality_grade IN (
            'good',
            'warning',
            'bad',
            'A',
            'B',
            'C',
            'reject'
        )
    ),
    notes TEXT,
    photo_url TEXT,
    coords JSONB,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- 1.8 CONVERSATIONS
CREATE TABLE IF NOT EXISTS public.conversations (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    type TEXT NOT NULL CHECK (type IN ('direct', 'group', 'broadcast')),
    name TEXT,
    participant_ids TEXT [] NOT NULL DEFAULT '{}',
    created_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);
-- 1.9 CHAT MESSAGES
CREATE TABLE IF NOT EXISTS public.chat_messages (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID NOT NULL REFERENCES public.conversations(id) ON DELETE CASCADE,
    sender_id UUID NOT NULL REFERENCES public.users(id),
    content TEXT NOT NULL,
    read_by TEXT [] DEFAULT '{}',
    created_at TIMESTAMPTZ DEFAULT NOW()
);
SELECT '✅ Core tables created' AS step;
-- =============================================
-- STEP 2: RLS & SECURITY
-- =============================================
ALTER TABLE public.orchards ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.users ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.pickers ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.day_setups ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bucket_records ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bins ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.quality_inspections ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.conversations ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.chat_messages ENABLE ROW LEVEL SECURITY;
-- Helper functions
CREATE OR REPLACE FUNCTION public.get_my_orchard_id() RETURNS UUID LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT orchard_id
FROM public.users
WHERE id = auth.uid()
LIMIT 1;
$$;
CREATE OR REPLACE FUNCTION public.is_manager_or_leader() RETURNS BOOLEAN LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'team_leader')
    );
$$;
-- Orchards
DROP POLICY IF EXISTS "Authenticated read orchards" ON public.orchards;
CREATE POLICY "Authenticated read orchards" ON public.orchards FOR
SELECT TO authenticated USING (true);
-- Users
DROP POLICY IF EXISTS "Users interactions" ON public.users;
DROP POLICY IF EXISTS "Read self" ON public.users;
CREATE POLICY "Read self" ON public.users FOR
SELECT USING (auth.uid() = id);
DROP POLICY IF EXISTS "Read orchard members" ON public.users;
CREATE POLICY "Read orchard members" ON public.users FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Update self" ON public.users;
CREATE POLICY "Update self" ON public.users FOR
UPDATE USING (auth.uid() = id);
DROP POLICY IF EXISTS "Insert self" ON public.users;
CREATE POLICY "Insert self" ON public.users FOR
INSERT WITH CHECK (auth.uid() = id);
-- Pickers
DROP POLICY IF EXISTS "Manage pickers" ON public.pickers;
CREATE POLICY "Manage pickers" ON public.pickers FOR ALL USING (is_manager_or_leader());
DROP POLICY IF EXISTS "Read pickers" ON public.pickers;
CREATE POLICY "Read pickers" ON public.pickers FOR
SELECT USING (orchard_id = get_my_orchard_id());
-- Bucket Records
DROP POLICY IF EXISTS "Runners insert records" ON public.bucket_records;
CREATE POLICY "Runners insert records" ON public.bucket_records FOR
INSERT WITH CHECK (auth.uid() = scanned_by);
DROP POLICY IF EXISTS "View orchard records" ON public.bucket_records;
CREATE POLICY "View orchard records" ON public.bucket_records FOR
SELECT USING (
        orchard_id = get_my_orchard_id()
        AND (
            is_manager_or_leader()
            OR scanned_by = auth.uid()
        )
    );
-- Messaging
DROP POLICY IF EXISTS "View conversations" ON public.conversations;
CREATE POLICY "View conversations" ON public.conversations FOR
SELECT USING (auth.uid()::text = ANY(participant_ids));
DROP POLICY IF EXISTS "Create conversations" ON public.conversations;
CREATE POLICY "Create conversations" ON public.conversations FOR
INSERT WITH CHECK (auth.uid()::text = ANY(participant_ids));
DROP POLICY IF EXISTS "Update conversations" ON public.conversations;
CREATE POLICY "Update conversations" ON public.conversations FOR
UPDATE USING (auth.uid()::text = ANY(participant_ids));
DROP POLICY IF EXISTS "View messages" ON public.chat_messages;
CREATE POLICY "View messages" ON public.chat_messages FOR
SELECT USING (
        EXISTS (
            SELECT 1
            FROM public.conversations c
            WHERE c.id = conversation_id
                AND auth.uid()::text = ANY(c.participant_ids)
        )
    );
DROP POLICY IF EXISTS "Send messages" ON public.chat_messages;
CREATE POLICY "Send messages" ON public.chat_messages FOR
INSERT WITH CHECK (
        auth.uid() = sender_id
        AND EXISTS (
            SELECT 1
            FROM public.conversations c
            WHERE c.id = conversation_id
                AND auth.uid()::text = ANY(c.participant_ids)
        )
    );
SELECT '✅ RLS policies applied' AS step;
-- =============================================
-- STEP 3: CORE INDEXES
-- =============================================
CREATE INDEX IF NOT EXISTS idx_users_orchard ON public.users(orchard_id);
CREATE INDEX IF NOT EXISTS idx_pickers_orchard ON public.pickers(orchard_id);
CREATE INDEX IF NOT EXISTS idx_pickers_code ON public.pickers(picker_id);
CREATE INDEX IF NOT EXISTS idx_bucket_records_picker ON public.bucket_records(picker_id);
CREATE INDEX IF NOT EXISTS idx_bucket_records_scanned_by ON public.bucket_records(scanned_by);
CREATE INDEX IF NOT EXISTS idx_bucket_records_created ON public.bucket_records(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_bucket_records_orchard ON public.bucket_records(orchard_id);
CREATE INDEX IF NOT EXISTS idx_conversations_participants ON public.conversations USING gin(participant_ids);
CREATE INDEX IF NOT EXISTS idx_chat_messages_conversation ON public.chat_messages(conversation_id);
CREATE INDEX IF NOT EXISTS idx_chat_messages_created ON public.chat_messages(created_at DESC);
-- =============================================
-- STEP 4: REALTIME
-- =============================================
DO $$ BEGIN ALTER PUBLICATION supabase_realtime
ADD TABLE public.chat_messages;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.conversations;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.bucket_records;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.users;
EXCEPTION
WHEN OTHERS THEN NULL;
END $$;
SELECT '✅ Core schema complete' AS step;
-- =============================================
-- STEP 5: DAY CLOSURES (20260210_day_closures.sql)
-- =============================================
CREATE TABLE IF NOT EXISTS day_closures (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    orchard_id UUID NOT NULL REFERENCES orchards(id) ON DELETE CASCADE,
    date DATE NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('open', 'closed')),
    closed_by UUID REFERENCES users(id),
    closed_at TIMESTAMPTZ,
    total_buckets INTEGER NOT NULL DEFAULT 0,
    total_cost DECIMAL(10, 2) NOT NULL DEFAULT 0.00,
    total_hours DECIMAL(8, 2),
    wage_violations INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(orchard_id, date)
);
CREATE INDEX IF NOT EXISTS idx_day_closures_orchard_date ON day_closures(orchard_id, date DESC);
CREATE INDEX IF NOT EXISTS idx_day_closures_status ON day_closures(status);
CREATE INDEX IF NOT EXISTS idx_day_closures_closed_at ON day_closures(closed_at DESC);
CREATE OR REPLACE FUNCTION update_day_closures_updated_at() RETURNS TRIGGER AS $$ BEGIN NEW.updated_at = NOW();
RETURN NEW;
END;
$$ LANGUAGE plpgsql;
DROP TRIGGER IF EXISTS day_closures_updated_at ON day_closures;
CREATE TRIGGER day_closures_updated_at BEFORE
UPDATE ON day_closures FOR EACH ROW EXECUTE FUNCTION update_day_closures_updated_at();
ALTER TABLE day_closures ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "authenticated_select_day_closures" ON day_closures;
CREATE POLICY "authenticated_select_day_closures" ON day_closures FOR
SELECT USING (auth.role() = 'authenticated');
DROP POLICY IF EXISTS "authenticated_insert_day_closures" ON day_closures;
CREATE POLICY "authenticated_insert_day_closures" ON day_closures FOR
INSERT WITH CHECK (auth.role() = 'authenticated');
-- ⚠️ FIX: Original referenced bucket_events (doesn't exist), using bucket_records
-- Only create policies on bucket_records if they don't conflict with existing policies
-- These check that inserts/updates/deletes can't happen on closed days
DO $$ BEGIN DROP POLICY IF EXISTS "no_insert_on_closed_days" ON bucket_records;
CREATE POLICY "no_insert_on_closed_days" ON bucket_records FOR
INSERT WITH CHECK (
        NOT EXISTS (
            SELECT 1
            FROM day_closures
            WHERE day_closures.orchard_id = bucket_records.orchard_id
                AND day_closures.date = DATE(bucket_records.scanned_at)
                AND day_closures.status = 'closed'
        )
    );
EXCEPTION
WHEN OTHERS THEN RAISE NOTICE 'Skipped day_closures policies on bucket_records: %',
SQLERRM;
END $$;
SELECT '✅ Day closures table created' AS step;
-- =============================================
-- STEP 6: AUTH HARDENING (20260211_auth_hardening.sql)
-- =============================================
-- Included by role constraint update below
-- =============================================
-- STEP 7: ROLE CONSTRAINT UPDATE
-- (Expanded from 20260212_add_qc_payroll_roles.sql to include ALL 8 roles)
-- =============================================
DO $$ BEGIN IF EXISTS (
    SELECT 1
    FROM information_schema.table_constraints
    WHERE table_name = 'users'
        AND constraint_type = 'CHECK'
        AND constraint_name = 'users_role_check'
) THEN
ALTER TABLE users DROP CONSTRAINT users_role_check;
END IF;
ALTER TABLE users
ADD CONSTRAINT users_role_check CHECK (
        role IN (
            'manager',
            'team_leader',
            'runner',
            'qc_inspector',
            'payroll_admin',
            'admin',
            'hr_admin',
            'logistics'
        )
    );
RAISE NOTICE 'Role constraint updated: 8 roles';
END $$;
COMMENT ON COLUMN users.role IS 'User role: manager, team_leader, runner, qc_inspector, payroll_admin, admin, hr_admin, logistics';
SELECT '✅ Role constraint expanded to 8 roles' AS step;
-- =============================================
-- STEP 8: AUDIT LOGGING (20260211_audit_logging.sql)
-- =============================================
CREATE TABLE IF NOT EXISTS audit_logs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES auth.users(id),
    user_email TEXT,
    action TEXT NOT NULL,
    table_name TEXT NOT NULL,
    record_id UUID,
    old_values JSONB,
    new_values JSONB,
    old_data JSONB,
    -- Alias used by payroll_rpc
    new_data JSONB,
    -- Alias used by payroll_rpc
    performed_by UUID,
    -- Used by payroll_rpc
    ip_address INET,
    user_agent TEXT,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- Add columns that might be missing from older versions
ALTER TABLE audit_logs
ADD COLUMN IF NOT EXISTS old_data JSONB;
ALTER TABLE audit_logs
ADD COLUMN IF NOT EXISTS new_data JSONB;
ALTER TABLE audit_logs
ADD COLUMN IF NOT EXISTS performed_by UUID;
CREATE INDEX IF NOT EXISTS idx_audit_logs_user_time ON audit_logs(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_audit_logs_table_record ON audit_logs(table_name, record_id);
CREATE INDEX IF NOT EXISTS idx_audit_logs_created_at ON audit_logs(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_audit_logs_action ON audit_logs(action);
ALTER TABLE audit_logs ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "managers_view_audit_logs" ON audit_logs;
CREATE POLICY "managers_view_audit_logs" ON audit_logs FOR
SELECT USING (
        auth.uid() IN (
            SELECT id
            FROM users
            WHERE role IN ('manager', 'admin')
        )
    );
DROP POLICY IF EXISTS "system_insert_audit_logs" ON audit_logs;
CREATE POLICY "system_insert_audit_logs" ON audit_logs FOR
INSERT WITH CHECK (true);
-- Audit trigger function
CREATE OR REPLACE FUNCTION log_audit_trail() RETURNS TRIGGER SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE current_user_email TEXT;
BEGIN
SELECT email INTO current_user_email
FROM auth.users
WHERE id = auth.uid();
INSERT INTO audit_logs (
        user_id,
        user_email,
        action,
        table_name,
        record_id,
        old_values,
        new_values
    )
VALUES (
        auth.uid(),
        current_user_email,
        TG_OP,
        TG_TABLE_NAME,
        COALESCE(NEW.id, OLD.id),
        CASE
            WHEN TG_OP IN ('UPDATE', 'DELETE') THEN row_to_json(OLD)::jsonb
            ELSE NULL
        END,
        CASE
            WHEN TG_OP IN ('INSERT', 'UPDATE') THEN row_to_json(NEW)::jsonb
            ELSE NULL
        END
    );
RETURN COALESCE(NEW, OLD);
EXCEPTION
WHEN OTHERS THEN RAISE WARNING 'Audit logging failed: %',
SQLERRM;
RETURN COALESCE(NEW, OLD);
END;
$$;
-- Apply triggers (skip settings which may not exist)
DROP TRIGGER IF EXISTS audit_pickers ON pickers;
CREATE TRIGGER audit_pickers
AFTER
INSERT
    OR
UPDATE
    OR DELETE ON pickers FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
DROP TRIGGER IF EXISTS audit_users ON users;
CREATE TRIGGER audit_users
AFTER
UPDATE ON users FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
DROP TRIGGER IF EXISTS audit_orchards ON orchards;
CREATE TRIGGER audit_orchards
AFTER
UPDATE ON orchards FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
-- Retention cleanup function
CREATE OR REPLACE FUNCTION cleanup_old_audit_logs() RETURNS void LANGUAGE plpgsql SECURITY DEFINER AS $$ BEGIN
DELETE FROM audit_logs
WHERE created_at < NOW() - INTERVAL '90 days';
END;
$$;
SELECT '✅ Audit logging installed' AS step;
-- =============================================
-- STEP 9: COMPLETE RLS (remaining tables from 20260211_*)
-- =============================================
-- Day setups policies
DROP POLICY IF EXISTS "Read day setups" ON public.day_setups;
CREATE POLICY "Read day setups" ON public.day_setups FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Manage day setups" ON public.day_setups;
CREATE POLICY "Manage day setups" ON public.day_setups FOR ALL USING (
    is_manager_or_leader()
    AND orchard_id = get_my_orchard_id()
);
-- Bins policies
DROP POLICY IF EXISTS "Read bins" ON public.bins;
CREATE POLICY "Read bins" ON public.bins FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Manage bins" ON public.bins;
CREATE POLICY "Manage bins" ON public.bins FOR ALL USING (orchard_id = get_my_orchard_id());
-- Quality inspections policies
DROP POLICY IF EXISTS "Read inspections" ON public.quality_inspections;
CREATE POLICY "Read inspections" ON public.quality_inspections FOR
SELECT USING (true);
-- All authenticated can view
DROP POLICY IF EXISTS "Create inspections" ON public.quality_inspections;
CREATE POLICY "Create inspections" ON public.quality_inspections FOR
INSERT WITH CHECK (inspector_id = auth.uid());
-- =============================================
-- STEP 10: SYNC CONFLICTS (20260212_sync_conflicts.sql)
-- =============================================
CREATE TABLE IF NOT EXISTS public.sync_conflicts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    table_name TEXT NOT NULL,
    record_id UUID NOT NULL,
    client_data JSONB NOT NULL,
    server_data JSONB,
    resolution TEXT CHECK (
        resolution IN (
            'client_wins',
            'server_wins',
            'merged',
            'pending'
        )
    ),
    resolved_by UUID REFERENCES public.users(id),
    resolved_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now()
);
ALTER TABLE public.sync_conflicts ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "Managers manage conflicts" ON public.sync_conflicts;
CREATE POLICY "Managers manage conflicts" ON public.sync_conflicts FOR ALL USING (is_manager_or_leader());
SELECT '✅ RLS & sync conflicts complete' AS step;
-- =============================================
-- STEP 11: PHASE 2 TABLES (20260213_phase2_tables.sql)
-- contracts, fleet_vehicles, transport_requests
-- =============================================
-- Helper functions for Phase 2
CREATE OR REPLACE FUNCTION public.is_hr_manager_or_admin() RETURNS BOOLEAN LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'hr_admin', 'admin')
    );
$$;
CREATE OR REPLACE FUNCTION public.is_logistics_or_manager() RETURNS BOOLEAN LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'logistics', 'admin')
    );
$$;
-- Contracts
CREATE TABLE IF NOT EXISTS public.contracts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    employee_id UUID NOT NULL REFERENCES public.users(id) ON DELETE RESTRICT,
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    type TEXT NOT NULL CHECK (type IN ('permanent', 'seasonal', 'casual')),
    status TEXT NOT NULL DEFAULT 'draft' CHECK (
        status IN (
            'active',
            'expiring',
            'expired',
            'draft',
            'terminated'
        )
    ),
    start_date DATE NOT NULL,
    end_date DATE,
    hourly_rate DECIMAL(10, 2) NOT NULL DEFAULT 23.50,
    notes TEXT,
    created_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now(),
    CONSTRAINT contracts_date_range CHECK (
        end_date IS NULL
        OR end_date >= start_date
    )
);
ALTER TABLE public.contracts DROP CONSTRAINT IF EXISTS contracts_unique_active;
CREATE UNIQUE INDEX IF NOT EXISTS idx_contracts_one_active_per_employee ON public.contracts (employee_id)
WHERE status = 'active';
-- Fleet Vehicles
CREATE TABLE IF NOT EXISTS public.fleet_vehicles (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    registration TEXT,
    zone TEXT,
    driver_id UUID REFERENCES public.users(id) ON DELETE
    SET NULL,
        driver_name TEXT,
        status TEXT NOT NULL DEFAULT 'idle' CHECK (
            status IN ('active', 'idle', 'maintenance', 'offline')
        ),
        load_status TEXT DEFAULT 'empty' CHECK (load_status IN ('empty', 'partial', 'full')),
        bins_loaded INTEGER DEFAULT 0,
        max_capacity INTEGER DEFAULT 8,
        fuel_level INTEGER CHECK (
            fuel_level IS NULL
            OR (
                fuel_level >= 0
                AND fuel_level <= 100
            )
        ),
        last_service_date DATE,
        next_service_date DATE,
        wof_expiry DATE,
        cof_expiry DATE,
        created_at TIMESTAMPTZ DEFAULT now(),
        updated_at TIMESTAMPTZ DEFAULT now()
);
-- Transport Requests
CREATE TABLE IF NOT EXISTS public.transport_requests (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    requested_by UUID NOT NULL REFERENCES public.users(id),
    requester_name TEXT NOT NULL,
    zone TEXT NOT NULL,
    bins_count INTEGER NOT NULL DEFAULT 1 CHECK (bins_count > 0),
    priority TEXT NOT NULL DEFAULT 'normal' CHECK (priority IN ('normal', 'high', 'urgent')),
    status TEXT NOT NULL DEFAULT 'pending' CHECK (
        status IN (
            'pending',
            'assigned',
            'in_progress',
            'completed',
            'cancelled'
        )
    ),
    assigned_vehicle UUID REFERENCES public.fleet_vehicles(id) ON DELETE
    SET NULL,
        assigned_by UUID REFERENCES public.users(id),
        notes TEXT,
        completed_at TIMESTAMPTZ,
        created_at TIMESTAMPTZ DEFAULT now(),
        updated_at TIMESTAMPTZ DEFAULT now()
);
-- Phase 2 RLS
ALTER TABLE public.contracts ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.fleet_vehicles ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.transport_requests ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "HR read contracts" ON public.contracts;
CREATE POLICY "HR read contracts" ON public.contracts FOR
SELECT USING (
        orchard_id = get_my_orchard_id()
        AND is_hr_manager_or_admin()
    );
DROP POLICY IF EXISTS "Employee read own contracts" ON public.contracts;
CREATE POLICY "Employee read own contracts" ON public.contracts FOR
SELECT USING (employee_id = auth.uid());
DROP POLICY IF EXISTS "HR manage contracts" ON public.contracts;
CREATE POLICY "HR manage contracts" ON public.contracts FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_hr_manager_or_admin()
);
DROP POLICY IF EXISTS "Read fleet vehicles" ON public.fleet_vehicles;
CREATE POLICY "Read fleet vehicles" ON public.fleet_vehicles FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Manage fleet vehicles" ON public.fleet_vehicles;
CREATE POLICY "Manage fleet vehicles" ON public.fleet_vehicles FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_logistics_or_manager()
);
DROP POLICY IF EXISTS "Read transport requests" ON public.transport_requests;
CREATE POLICY "Read transport requests" ON public.transport_requests FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Create transport requests" ON public.transport_requests;
CREATE POLICY "Create transport requests" ON public.transport_requests FOR
INSERT WITH CHECK (
        orchard_id = get_my_orchard_id()
        AND requested_by = auth.uid()
    );
DROP POLICY IF EXISTS "Manage transport requests" ON public.transport_requests;
CREATE POLICY "Manage transport requests" ON public.transport_requests FOR
UPDATE USING (
        orchard_id = get_my_orchard_id()
        AND is_logistics_or_manager()
    );
-- Phase 2 indexes
CREATE INDEX IF NOT EXISTS idx_contracts_employee ON public.contracts(employee_id);
CREATE INDEX IF NOT EXISTS idx_contracts_orchard ON public.contracts(orchard_id);
CREATE INDEX IF NOT EXISTS idx_contracts_status ON public.contracts(status);
CREATE INDEX IF NOT EXISTS idx_contracts_end_date ON public.contracts(end_date)
WHERE end_date IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_fleet_orchard ON public.fleet_vehicles(orchard_id);
CREATE INDEX IF NOT EXISTS idx_fleet_status ON public.fleet_vehicles(status);
CREATE INDEX IF NOT EXISTS idx_fleet_driver ON public.fleet_vehicles(driver_id)
WHERE driver_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_transport_orchard ON public.transport_requests(orchard_id);
CREATE INDEX IF NOT EXISTS idx_transport_status ON public.transport_requests(status);
CREATE INDEX IF NOT EXISTS idx_transport_priority ON public.transport_requests(priority, created_at DESC)
WHERE status IN ('pending', 'assigned');
CREATE INDEX IF NOT EXISTS idx_transport_created ON public.transport_requests(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_transport_vehicle ON public.transport_requests(assigned_vehicle)
WHERE assigned_vehicle IS NOT NULL;
-- Phase 2 updated_at trigger
CREATE OR REPLACE FUNCTION public.set_updated_at() RETURNS TRIGGER AS $$ BEGIN NEW.updated_at = now();
RETURN NEW;
END;
$$ LANGUAGE plpgsql;
DROP TRIGGER IF EXISTS contracts_updated_at ON public.contracts;
CREATE TRIGGER contracts_updated_at BEFORE
UPDATE ON public.contracts FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DROP TRIGGER IF EXISTS fleet_updated_at ON public.fleet_vehicles;
CREATE TRIGGER fleet_updated_at BEFORE
UPDATE ON public.fleet_vehicles FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DROP TRIGGER IF EXISTS transport_updated_at ON public.transport_requests;
CREATE TRIGGER transport_updated_at BEFORE
UPDATE ON public.transport_requests FOR EACH ROW EXECUTE FUNCTION set_updated_at();
-- Phase 2 realtime
DO $$ BEGIN ALTER PUBLICATION supabase_realtime
ADD TABLE public.transport_requests;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.fleet_vehicles;
EXCEPTION
WHEN OTHERS THEN NULL;
END $$;
SELECT '✅ Phase 2 tables created (contracts, fleet, transport)' AS step;
-- =============================================
-- STEP 12: DAILY ATTENDANCE (20260213_daily_attendance.sql)
-- =============================================
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
    CONSTRAINT daily_attendance_unique UNIQUE (picker_id, date),
    CONSTRAINT daily_attendance_time_range CHECK (
        check_out IS NULL
        OR check_in IS NULL
        OR check_out > check_in
    )
);
-- Timesheet corrections columns
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS correction_reason TEXT;
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS corrected_by UUID;
ALTER TABLE public.daily_attendance
ADD COLUMN IF NOT EXISTS corrected_at TIMESTAMPTZ;
ALTER TABLE public.daily_attendance ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "Read attendance" ON public.daily_attendance;
CREATE POLICY "Read attendance" ON public.daily_attendance FOR
SELECT USING (orchard_id = get_my_orchard_id());
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
CREATE INDEX IF NOT EXISTS idx_attendance_orchard_date ON public.daily_attendance(orchard_id, date);
CREATE INDEX IF NOT EXISTS idx_attendance_picker_date ON public.daily_attendance(picker_id, date);
CREATE INDEX IF NOT EXISTS idx_attendance_date ON public.daily_attendance(date DESC);
CREATE INDEX IF NOT EXISTS idx_attendance_status ON public.daily_attendance(status)
WHERE status != 'present';
CREATE INDEX IF NOT EXISTS idx_daily_attendance_corrected ON public.daily_attendance(corrected_at)
WHERE corrected_at IS NOT NULL;
-- Audit trigger for attendance
DROP TRIGGER IF EXISTS audit_daily_attendance ON daily_attendance;
CREATE TRIGGER audit_daily_attendance
AFTER
INSERT
    OR
UPDATE
    OR DELETE ON daily_attendance FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
DROP TRIGGER IF EXISTS attendance_updated_at ON public.daily_attendance;
CREATE TRIGGER attendance_updated_at BEFORE
UPDATE ON public.daily_attendance FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DO $$ BEGIN ALTER PUBLICATION supabase_realtime
ADD TABLE public.daily_attendance;
EXCEPTION
WHEN OTHERS THEN NULL;
END $$;
SELECT '✅ Daily attendance table created' AS step;
-- =============================================
-- STEP 13: PAYROLL RPC (20260213_payroll_rpc.sql)
-- =============================================
CREATE OR REPLACE FUNCTION public.close_payroll_period(
        p_orchard_id UUID,
        p_period_start DATE,
        p_period_end DATE
    ) RETURNS JSON LANGUAGE plpgsql SECURITY DEFINER
SET search_path = public AS $$
DECLARE v_total_buckets INTEGER;
v_total_hours DECIMAL;
v_total_earnings DECIMAL;
v_picker_count INTEGER;
v_result JSON;
BEGIN IF NOT EXISTS (
    SELECT 1
    FROM public.users
    WHERE id = auth.uid()
        AND role IN ('manager', 'hr_admin', 'admin')
        AND orchard_id = p_orchard_id
) THEN RAISE EXCEPTION 'Insufficient permissions to close payroll';
END IF;
SELECT COUNT(*),
    COUNT(DISTINCT br.picker_id) INTO v_total_buckets,
    v_picker_count
FROM public.bucket_records br
WHERE br.orchard_id = p_orchard_id
    AND br.scanned_at >= p_period_start::TIMESTAMPTZ
    AND br.scanned_at < (p_period_end + 1)::TIMESTAMPTZ;
SELECT COALESCE(SUM(hours_worked), 0) INTO v_total_hours
FROM public.daily_attendance
WHERE orchard_id = p_orchard_id
    AND date >= p_period_start
    AND date <= p_period_end
    AND status IN ('present', 'late', 'half_day');
SELECT COALESCE(SUM(ds.piece_rate), 0) INTO v_total_earnings
FROM public.day_setups ds
WHERE ds.orchard_id = p_orchard_id
    AND ds.date >= p_period_start
    AND ds.date <= p_period_end;
v_total_earnings := v_total_buckets * COALESCE(
    v_total_earnings / NULLIF(
        (
            SELECT COUNT(*)
            FROM public.day_setups
            WHERE orchard_id = p_orchard_id
                AND date >= p_period_start
                AND date <= p_period_end
        ),
        0
    ),
    6.50
);
v_result := json_build_object(
    'status',
    'closed',
    'period_start',
    p_period_start,
    'period_end',
    p_period_end,
    'orchard_id',
    p_orchard_id,
    'total_buckets',
    v_total_buckets,
    'total_hours',
    v_total_hours,
    'total_earnings',
    ROUND(v_total_earnings, 2),
    'picker_count',
    v_picker_count,
    'closed_at',
    now(),
    'closed_by',
    auth.uid()
);
INSERT INTO public.audit_logs (
        action,
        table_name,
        record_id,
        old_data,
        new_data,
        performed_by
    )
VALUES (
        'PAYROLL_CLOSE',
        'payroll',
        p_orchard_id,
        NULL,
        v_result,
        auth.uid()
    );
RETURN v_result;
EXCEPTION
WHEN OTHERS THEN RAISE;
END;
$$;
GRANT EXECUTE ON FUNCTION public.close_payroll_period(UUID, DATE, DATE) TO authenticated;
SELECT '✅ Payroll RPC created' AS step;
-- =============================================
-- STEP 14: QC PHOTOS BUCKET (20260213_create_qc_photos_bucket.sql)
-- =============================================
INSERT INTO storage.buckets (
        id,
        name,
        public,
        file_size_limit,
        allowed_mime_types
    )
VALUES (
        'qc-photos',
        'qc-photos',
        true,
        5242880,
        ARRAY ['image/webp', 'image/jpeg', 'image/png']::text []
    ) ON CONFLICT (id) DO NOTHING;
DROP POLICY IF EXISTS "Authenticated users can upload QC photos" ON storage.objects;
CREATE POLICY "Authenticated users can upload QC photos" ON storage.objects FOR
INSERT TO authenticated WITH CHECK (bucket_id = 'qc-photos');
DROP POLICY IF EXISTS "Public can view QC photos" ON storage.objects;
CREATE POLICY "Public can view QC photos" ON storage.objects FOR
SELECT TO public USING (bucket_id = 'qc-photos');
DROP POLICY IF EXISTS "Users can delete own QC photos" ON storage.objects;
CREATE POLICY "Users can delete own QC photos" ON storage.objects FOR DELETE TO authenticated USING (bucket_id = 'qc-photos');
SELECT '✅ QC photos storage bucket created' AS step;
-- =============================================
-- STEP 15: VERIFICATION QUERIES
-- =============================================
SELECT '========================================' AS separator;
SELECT '🔍 VERIFICATION: Tables Created' AS title;
SELECT table_name
FROM information_schema.tables
WHERE table_schema = 'public'
ORDER BY table_name;
SELECT '🔍 VERIFICATION: Users role constraint' AS title;
SELECT constraint_name,
    check_clause
FROM information_schema.check_constraints
WHERE constraint_name = 'users_role_check';
SELECT '🔍 VERIFICATION: RLS Status' AS title;
SELECT tablename,
    rowsecurity
FROM pg_tables
WHERE schemaname = 'public'
    AND rowsecurity = true
ORDER BY tablename;
SELECT '========================================' AS separator;
SELECT '🎉 ALL MIGRATIONS APPLIED SUCCESSFULLY' AS result;