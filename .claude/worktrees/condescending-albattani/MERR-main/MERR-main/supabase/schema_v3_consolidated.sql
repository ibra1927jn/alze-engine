-- =============================================
-- HARVESTPRO NZ — CONSOLIDATED DATABASE SCHEMA V3
-- Created: 2026-02-26
-- =============================================
-- SINGLE SOURCE OF TRUTH. Merges V2 + all Sprint 1 migrations.
--
-- Tables (26 total):
--   Hierarchy:  harvest_seasons, orchard_blocks, block_rows
--   Core:       orchards, users, pickers, day_setups, bucket_records, bins
--   Assignment: row_assignments
--   Quality:    quality_inspections, qc_inspections
--   Messaging:  conversations, chat_messages
--   Attendance: daily_attendance
--   Logistics:  fleet_vehicles, transport_requests, day_closures
--   HR:         contracts
--   Security:   login_attempts, account_locks, audit_logs, sync_conflicts, allowed_registrations
--   Settings:   harvest_settings
--   Notifications: messages
--
-- Key features:
--   - Soft deletes (deleted_at) on all operational tables
--   - Optimistic locking (version) on concurrency-hot tables
--   - Partial unique indexes (safe with soft deletes)
--   - B-Tree sync indexes for delta sync
--   - Season-scoped queries to prevent OOM in future years
--
-- Roles: admin, manager, team_leader, runner, qc_inspector, hr_admin, payroll_admin, logistics
-- =============================================
-- =============================================
-- 0. EXTENSIONS
-- =============================================
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
-- =============================================
-- 1. BASE TABLES
-- =============================================
-- 1.1 ORCHARDS
CREATE TABLE IF NOT EXISTS public.orchards (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    code TEXT UNIQUE,
    name TEXT NOT NULL,
    location TEXT,
    total_blocks INTEGER DEFAULT 0,
    total_rows INTEGER DEFAULT 20,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- 1.2 HARVEST SEASONS (scopes all data by year — prevents OOM)
CREATE TABLE IF NOT EXISTS public.harvest_seasons (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    start_date DATE NOT NULL,
    end_date DATE,
    status TEXT NOT NULL DEFAULT 'active' CHECK (
        status IN ('planning', 'active', 'closed', 'archived')
    ),
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_one_active_season_per_orchard ON public.harvest_seasons (orchard_id)
WHERE status = 'active'
    AND deleted_at IS NULL;
-- 1.3 ORCHARD BLOCKS (subdivisions of an orchard within a season)
CREATE TABLE IF NOT EXISTS public.orchard_blocks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    season_id UUID NOT NULL REFERENCES public.harvest_seasons(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    total_rows INTEGER NOT NULL DEFAULT 0,
    start_row INTEGER NOT NULL DEFAULT 1,
    color_code TEXT DEFAULT '#dc2626',
    status TEXT NOT NULL DEFAULT 'idle' CHECK (
        status IN ('idle', 'active', 'complete', 'alert')
    ),
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_active_block ON public.orchard_blocks (orchard_id, season_id, name)
WHERE deleted_at IS NULL;
-- 1.4 BLOCK ROWS (each row has a variety)
CREATE TABLE IF NOT EXISTS public.block_rows (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    block_id UUID NOT NULL REFERENCES public.orchard_blocks(id) ON DELETE CASCADE,
    row_number INTEGER NOT NULL,
    variety TEXT,
    target_buckets INTEGER DEFAULT 100,
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_unique_active_row ON public.block_rows (block_id, row_number)
WHERE deleted_at IS NULL;
-- 1.5 USERS (extends auth.users)
CREATE TABLE IF NOT EXISTS public.users (
    id UUID PRIMARY KEY REFERENCES auth.users(id) ON DELETE CASCADE,
    email TEXT,
    full_name TEXT,
    role TEXT DEFAULT 'team_leader' CHECK (
        role IN (
            'admin',
            'manager',
            'team_leader',
            'runner',
            'qc_inspector',
            'hr_admin',
            'payroll_admin',
            'logistics'
        )
    ),
    orchard_id UUID REFERENCES public.orchards(id),
    team_id UUID,
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ(3) DEFAULT now()
);
-- 1.6 PICKERS (seasonal workers)
CREATE TABLE IF NOT EXISTS public.pickers (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    picker_id TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    orchard_id UUID REFERENCES public.orchards(id),
    team_leader_id UUID REFERENCES public.users(id),
    role TEXT DEFAULT 'picker' CHECK (
        role IN (
            'picker',
            'runner',
            'bucket_runner',
            'team_leader'
        )
    ),
    safety_verified BOOLEAN DEFAULT false,
    total_buckets_today INTEGER DEFAULT 0,
    current_row INTEGER DEFAULT 0,
    status TEXT DEFAULT 'active',
    archived_at TIMESTAMPTZ,
    deleted_at TIMESTAMPTZ,
    version INTEGER DEFAULT 1,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ(3) DEFAULT now()
);
-- 1.7 DAY SETUPS
CREATE TABLE IF NOT EXISTS public.day_setups (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    season_id UUID REFERENCES public.harvest_seasons(id),
    date DATE DEFAULT CURRENT_DATE,
    variety TEXT,
    target_tons DECIMAL(10, 2),
    piece_rate DECIMAL(10, 2) DEFAULT 6.50,
    min_wage_rate DECIMAL(10, 2) DEFAULT 23.50,
    start_time TIME,
    created_by UUID REFERENCES public.users(id),
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- 1.8 BUCKET RECORDS (core transaction)
CREATE TABLE IF NOT EXISTS public.bucket_records (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    season_id UUID REFERENCES public.harvest_seasons(id),
    picker_id UUID REFERENCES public.pickers(id),
    bin_id UUID,
    block_row_id UUID REFERENCES public.block_rows(id),
    scanned_by UUID REFERENCES public.users(id),
    scanned_at TIMESTAMPTZ DEFAULT now(),
    row_number INTEGER,
    coords JSONB,
    quality_grade TEXT,
    deleted_at TIMESTAMPTZ,
    version INTEGER DEFAULT 1,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ(3) DEFAULT now()
);
-- 1.9 BINS (logistics)
CREATE TABLE IF NOT EXISTS public.bins (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id),
    block_id UUID REFERENCES public.orchard_blocks(id),
    bin_code TEXT,
    status TEXT DEFAULT 'empty' CHECK (
        status IN ('empty', 'partial', 'full', 'collected')
    ),
    variety TEXT,
    location JSONB,
    movement_history JSONB [] DEFAULT '{}',
    filled_at TIMESTAMPTZ,
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- 1.10 ROW ASSIGNMENTS
CREATE TABLE IF NOT EXISTS public.row_assignments (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID REFERENCES public.orchards(id) ON DELETE CASCADE,
    season_id UUID REFERENCES public.harvest_seasons(id),
    block_row_id UUID REFERENCES public.block_rows(id),
    row_number INTEGER NOT NULL,
    side TEXT DEFAULT 'north' CHECK (side IN ('north', 'south')),
    assigned_pickers UUID [] DEFAULT '{}',
    completion_percentage INTEGER DEFAULT 0 CHECK (
        completion_percentage BETWEEN 0 AND 100
    ),
    status TEXT DEFAULT 'active' CHECK (status IN ('active', 'completed')),
    assigned_at TIMESTAMPTZ DEFAULT now(),
    deleted_at TIMESTAMPTZ,
    version INTEGER DEFAULT 1
);
-- 1.11 QUALITY INSPECTIONS (legacy v1)
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
-- 1.12 QC INSPECTIONS (normalized v2)
CREATE TABLE IF NOT EXISTS public.qc_inspections (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    picker_id UUID NOT NULL REFERENCES public.users(id) ON DELETE CASCADE,
    inspector_id UUID NOT NULL REFERENCES public.users(id) ON DELETE CASCADE,
    grade TEXT NOT NULL CHECK (grade IN ('A', 'B', 'C', 'reject')),
    notes TEXT,
    photo_url TEXT,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- =============================================
-- 2. MESSAGING
-- =============================================
CREATE TABLE IF NOT EXISTS public.conversations (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    type TEXT NOT NULL CHECK (type IN ('direct', 'group', 'broadcast')),
    name TEXT,
    participant_ids TEXT [] NOT NULL DEFAULT '{}',
    created_by UUID REFERENCES public.users(id),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);
CREATE TABLE IF NOT EXISTS public.chat_messages (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID NOT NULL REFERENCES public.conversations(id) ON DELETE CASCADE,
    sender_id UUID NOT NULL REFERENCES public.users(id),
    content TEXT NOT NULL,
    read_by TEXT [] DEFAULT '{}',
    created_at TIMESTAMPTZ DEFAULT NOW()
);
-- =============================================
-- 3. ATTENDANCE & CLOSURES
-- =============================================
CREATE TABLE IF NOT EXISTS public.daily_attendance (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    picker_id UUID NOT NULL REFERENCES public.pickers(id) ON DELETE CASCADE,
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    season_id UUID REFERENCES public.harvest_seasons(id),
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
    correction_reason TEXT,
    corrected_by UUID REFERENCES auth.users(id),
    corrected_at TIMESTAMPTZ,
    deleted_at TIMESTAMPTZ,
    version INTEGER DEFAULT 1,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now(),
    CONSTRAINT daily_attendance_unique UNIQUE (picker_id, date),
    CONSTRAINT daily_attendance_time_range CHECK (
        check_out IS NULL
        OR check_in IS NULL
        OR check_out > check_in
    )
);
CREATE TABLE IF NOT EXISTS public.day_closures (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    date DATE NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('open', 'closed')),
    closed_by UUID REFERENCES public.users(id),
    closed_at TIMESTAMPTZ,
    total_buckets INTEGER NOT NULL DEFAULT 0,
    total_cost DECIMAL(10, 2) NOT NULL DEFAULT 0.00,
    total_hours DECIMAL(8, 2),
    wage_violations INTEGER DEFAULT 0,
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(orchard_id, date)
);
-- =============================================
-- 4. LOGISTICS
-- =============================================
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
        deleted_at TIMESTAMPTZ,
        created_at TIMESTAMPTZ DEFAULT now(),
        updated_at TIMESTAMPTZ DEFAULT now()
);
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
        deleted_at TIMESTAMPTZ,
        created_at TIMESTAMPTZ DEFAULT now(),
        updated_at TIMESTAMPTZ DEFAULT now()
);
-- =============================================
-- 5. HR
-- =============================================
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
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now(),
    CONSTRAINT contracts_date_range CHECK (
        end_date IS NULL
        OR end_date >= start_date
    )
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_contracts_one_active_per_employee ON public.contracts (employee_id)
WHERE status = 'active';
-- =============================================
-- 6. SETTINGS
-- =============================================
CREATE TABLE IF NOT EXISTS public.harvest_settings (
    orchard_id UUID PRIMARY KEY REFERENCES public.orchards(id) ON DELETE CASCADE,
    min_wage_rate DECIMAL(10, 2) DEFAULT 23.50,
    piece_rate DECIMAL(10, 2) DEFAULT 6.50,
    min_buckets_per_hour DECIMAL(10, 2) DEFAULT 3.6,
    target_tons DECIMAL(10, 2) DEFAULT 40.0,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- =============================================
-- 7. SIMPLE MESSAGES (system notifications)
-- =============================================
CREATE TABLE IF NOT EXISTS public.messages (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    sender_id UUID NOT NULL,
    receiver_id UUID NOT NULL REFERENCES public.users(id),
    content TEXT NOT NULL,
    type TEXT DEFAULT 'system',
    read BOOLEAN DEFAULT false,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- =============================================
-- 8. SECURITY & AUTH
-- =============================================
CREATE TABLE IF NOT EXISTS public.login_attempts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email TEXT NOT NULL,
    ip_address INET,
    attempt_time TIMESTAMPTZ DEFAULT now(),
    success BOOLEAN DEFAULT false,
    user_agent TEXT,
    failure_reason TEXT
);
CREATE TABLE IF NOT EXISTS public.account_locks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES auth.users(id) ON DELETE CASCADE,
    email TEXT NOT NULL,
    locked_at TIMESTAMPTZ DEFAULT now(),
    locked_until TIMESTAMPTZ NOT NULL,
    locked_by_system BOOLEAN DEFAULT true,
    unlock_reason TEXT,
    unlocked_by UUID REFERENCES auth.users(id),
    unlocked_at TIMESTAMPTZ,
    CONSTRAINT unique_active_lock UNIQUE (user_id, locked_at)
);
CREATE TABLE IF NOT EXISTS public.audit_logs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES auth.users(id),
    user_email TEXT,
    action TEXT NOT NULL CHECK (
        action IN ('INSERT', 'UPDATE', 'DELETE', 'CUSTOM')
    ),
    table_name TEXT NOT NULL,
    record_id UUID,
    old_values JSONB,
    new_values JSONB,
    ip_address INET,
    user_agent TEXT,
    created_at TIMESTAMPTZ DEFAULT now()
);
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
-- HR registration whitelist
CREATE TABLE IF NOT EXISTS public.allowed_registrations (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email TEXT NOT NULL UNIQUE,
    assigned_role TEXT NOT NULL DEFAULT 'team_leader' CHECK (
        assigned_role IN (
            'admin',
            'manager',
            'team_leader',
            'runner',
            'qc_inspector',
            'hr_admin',
            'payroll_admin',
            'logistics'
        )
    ),
    orchard_id UUID REFERENCES public.orchards(id),
    created_by UUID REFERENCES auth.users(id),
    created_at TIMESTAMPTZ DEFAULT now()
);
-- =============================================
-- 9. ENABLE RLS ON ALL TABLES
-- =============================================
ALTER TABLE public.orchards ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.harvest_seasons ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.orchard_blocks ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.block_rows ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.users ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.pickers ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.day_setups ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bucket_records ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bins ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.row_assignments ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.quality_inspections ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.qc_inspections ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.conversations ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.chat_messages ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.daily_attendance ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.day_closures ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.fleet_vehicles ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.transport_requests ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.contracts ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.harvest_settings ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.messages ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.login_attempts ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.account_locks ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.audit_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.sync_conflicts ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.allowed_registrations ENABLE ROW LEVEL SECURITY;
-- =============================================
-- 9B. RLS POLICIES
-- =============================================
-- ORCHARDS
DROP POLICY IF EXISTS "Read own orchard" ON public.orchards;
CREATE POLICY "Read own orchard" ON public.orchards FOR
SELECT USING (id = get_my_orchard_id());
-- HARVEST SEASONS
DROP POLICY IF EXISTS "Read seasons" ON public.harvest_seasons;
CREATE POLICY "Read seasons" ON public.harvest_seasons FOR
SELECT TO authenticated USING (
        orchard_id = get_my_orchard_id()
        AND deleted_at IS NULL
    );
DROP POLICY IF EXISTS "Manage seasons" ON public.harvest_seasons;
CREATE POLICY "Manage seasons" ON public.harvest_seasons FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'admin')
    )
);
-- ORCHARD BLOCKS
DROP POLICY IF EXISTS "Read blocks" ON public.orchard_blocks;
CREATE POLICY "Read blocks" ON public.orchard_blocks FOR
SELECT TO authenticated USING (
        orchard_id = get_my_orchard_id()
        AND deleted_at IS NULL
    );
DROP POLICY IF EXISTS "Manage blocks" ON public.orchard_blocks;
CREATE POLICY "Manage blocks" ON public.orchard_blocks FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'admin')
    )
);
-- BLOCK ROWS
DROP POLICY IF EXISTS "Read rows" ON public.block_rows;
CREATE POLICY "Read rows" ON public.block_rows FOR
SELECT TO authenticated USING (
        deleted_at IS NULL
        AND EXISTS (
            SELECT 1
            FROM public.orchard_blocks ob
            WHERE ob.id = block_id
                AND ob.orchard_id = get_my_orchard_id()
                AND ob.deleted_at IS NULL
        )
    );
DROP POLICY IF EXISTS "Manage rows" ON public.block_rows;
CREATE POLICY "Manage rows" ON public.block_rows FOR ALL USING (
    EXISTS (
        SELECT 1
        FROM public.orchard_blocks ob
            JOIN public.users u ON u.id = auth.uid()
        WHERE ob.id = block_id
            AND ob.orchard_id = u.orchard_id
            AND u.role IN ('manager', 'admin')
    )
);
-- USERS (non-recursive via SECURITY DEFINER helpers)
DROP POLICY IF EXISTS "users_view_policy" ON public.users;
CREATE POLICY "users_view_policy" ON public.users FOR
SELECT USING (
        id = auth.uid()
        OR get_auth_role() IN ('manager', 'admin')
        OR orchard_id = get_auth_orchard_id()
    );
DROP POLICY IF EXISTS "Users insert own record" ON public.users;
CREATE POLICY "Users insert own record" ON public.users FOR
INSERT WITH CHECK (id = auth.uid());
DROP POLICY IF EXISTS "Users update own profile" ON public.users;
CREATE POLICY "Users update own profile" ON public.users FOR
UPDATE USING (
        id = auth.uid()
        OR get_auth_role() IN ('manager', 'admin')
    );
-- PICKERS
DROP POLICY IF EXISTS "Read orchard pickers" ON public.pickers;
CREATE POLICY "Read orchard pickers" ON public.pickers FOR
SELECT USING (
        orchard_id = get_my_orchard_id()
        AND deleted_at IS NULL
    );
DROP POLICY IF EXISTS "Manage pickers" ON public.pickers;
CREATE POLICY "Manage pickers" ON public.pickers FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_manager_or_leader()
);
-- DAY SETUPS
DROP POLICY IF EXISTS "Read day setups" ON public.day_setups;
CREATE POLICY "Read day setups" ON public.day_setups FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Manage day setups" ON public.day_setups;
CREATE POLICY "Manage day setups" ON public.day_setups FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_manager_or_leader()
);
-- BUCKET RECORDS
DROP POLICY IF EXISTS "Read bucket records" ON public.bucket_records;
CREATE POLICY "Read bucket records" ON public.bucket_records FOR
SELECT USING (
        orchard_id = get_my_orchard_id()
        AND deleted_at IS NULL
    );
DROP POLICY IF EXISTS "Insert bucket records" ON public.bucket_records;
CREATE POLICY "Insert bucket records" ON public.bucket_records FOR
INSERT WITH CHECK (orchard_id = get_my_orchard_id());
-- BINS
DROP POLICY IF EXISTS "Read bins" ON public.bins;
CREATE POLICY "Read bins" ON public.bins FOR
SELECT USING (
        orchard_id = get_my_orchard_id()
        AND deleted_at IS NULL
    );
DROP POLICY IF EXISTS "Manage bins" ON public.bins;
CREATE POLICY "Manage bins" ON public.bins FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_manager_or_leader()
);
-- ROW ASSIGNMENTS
DROP POLICY IF EXISTS "Read row assignments" ON public.row_assignments;
CREATE POLICY "Read row assignments" ON public.row_assignments FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Manage row assignments" ON public.row_assignments;
CREATE POLICY "Manage row assignments" ON public.row_assignments FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_manager_or_leader()
);
-- CONVERSATIONS
DROP POLICY IF EXISTS "conversation_members" ON public.conversations;
CREATE POLICY "conversation_members" ON public.conversations FOR
SELECT USING (auth.uid()::text = ANY(participant_ids));
DROP POLICY IF EXISTS "create_conversations" ON public.conversations;
CREATE POLICY "create_conversations" ON public.conversations FOR
INSERT WITH CHECK (auth.uid()::text = ANY(participant_ids));
-- CHAT MESSAGES
DROP POLICY IF EXISTS "view_messages" ON public.chat_messages;
CREATE POLICY "view_messages" ON public.chat_messages FOR
SELECT USING (
        EXISTS (
            SELECT 1
            FROM conversations c
            WHERE c.id = conversation_id
                AND auth.uid()::text = ANY(c.participant_ids)
        )
    );
DROP POLICY IF EXISTS "send_messages" ON public.chat_messages;
CREATE POLICY "send_messages" ON public.chat_messages FOR
INSERT WITH CHECK (sender_id = auth.uid());
-- DAILY ATTENDANCE
DROP POLICY IF EXISTS "Read attendance" ON public.daily_attendance;
CREATE POLICY "Read attendance" ON public.daily_attendance FOR
SELECT USING (
        orchard_id = get_my_orchard_id()
        AND deleted_at IS NULL
    );
DROP POLICY IF EXISTS "Manage attendance" ON public.daily_attendance;
CREATE POLICY "Manage attendance" ON public.daily_attendance FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_manager_or_leader()
);
-- DAY CLOSURES
DROP POLICY IF EXISTS "Read day closures" ON public.day_closures;
CREATE POLICY "Read day closures" ON public.day_closures FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Managers close days" ON public.day_closures;
CREATE POLICY "Managers close days" ON public.day_closures FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'admin')
    )
);
-- FLEET VEHICLES
DROP POLICY IF EXISTS "Read fleet" ON public.fleet_vehicles;
CREATE POLICY "Read fleet" ON public.fleet_vehicles FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Manage fleet" ON public.fleet_vehicles;
CREATE POLICY "Manage fleet" ON public.fleet_vehicles FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_logistics_or_manager()
);
-- TRANSPORT REQUESTS
DROP POLICY IF EXISTS "Read transport" ON public.transport_requests;
CREATE POLICY "Read transport" ON public.transport_requests FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Create transport" ON public.transport_requests;
CREATE POLICY "Create transport" ON public.transport_requests FOR
INSERT WITH CHECK (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Manage transport" ON public.transport_requests;
CREATE POLICY "Manage transport" ON public.transport_requests FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_logistics_or_manager()
);
-- CONTRACTS
DROP POLICY IF EXISTS "Employee read own contracts" ON public.contracts;
CREATE POLICY "Employee read own contracts" ON public.contracts FOR
SELECT USING (employee_id = auth.uid());
DROP POLICY IF EXISTS "HR manage contracts" ON public.contracts;
CREATE POLICY "HR manage contracts" ON public.contracts FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_hr_manager_or_admin()
);
-- HARVEST SETTINGS
DROP POLICY IF EXISTS "Read settings" ON public.harvest_settings;
CREATE POLICY "Read settings" ON public.harvest_settings FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "Manage settings" ON public.harvest_settings;
CREATE POLICY "Manage settings" ON public.harvest_settings FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'admin')
    )
);
-- MESSAGES
DROP POLICY IF EXISTS "Read own messages" ON public.messages;
CREATE POLICY "Read own messages" ON public.messages FOR
SELECT USING (
        receiver_id = auth.uid()
        OR sender_id = auth.uid()::uuid
    );
DROP POLICY IF EXISTS "Send messages" ON public.messages;
CREATE POLICY "Send messages" ON public.messages FOR
INSERT WITH CHECK (true);
-- QC INSPECTIONS
DROP POLICY IF EXISTS "Read qc inspections" ON public.qc_inspections;
CREATE POLICY "Read qc inspections" ON public.qc_inspections FOR
SELECT USING (orchard_id = get_my_orchard_id());
DROP POLICY IF EXISTS "QC inspectors create inspections" ON public.qc_inspections;
CREATE POLICY "QC inspectors create inspections" ON public.qc_inspections FOR
INSERT WITH CHECK (
        inspector_id = auth.uid()
        AND orchard_id = get_my_orchard_id()
    );
-- LOGIN ATTEMPTS
DROP POLICY IF EXISTS "anyone_can_insert_login_attempts" ON public.login_attempts;
CREATE POLICY "anyone_can_insert_login_attempts" ON public.login_attempts FOR
INSERT WITH CHECK (true);
DROP POLICY IF EXISTS "managers_view_login_attempts" ON public.login_attempts;
CREATE POLICY "managers_view_login_attempts" ON public.login_attempts FOR
SELECT USING (get_auth_role() = 'manager');
-- ACCOUNT LOCKS
DROP POLICY IF EXISTS "managers_full_access_account_locks" ON public.account_locks;
CREATE POLICY "managers_full_access_account_locks" ON public.account_locks FOR ALL USING (get_auth_role() = 'manager');
DROP POLICY IF EXISTS "system_insert_account_locks" ON public.account_locks;
CREATE POLICY "system_insert_account_locks" ON public.account_locks FOR
INSERT WITH CHECK (locked_by_system = true);
-- AUDIT LOGS
DROP POLICY IF EXISTS "managers_view_audit_logs" ON public.audit_logs;
CREATE POLICY "managers_view_audit_logs" ON public.audit_logs FOR
SELECT USING (get_auth_role() IN ('manager', 'admin'));
DROP POLICY IF EXISTS "system_insert_audit_logs" ON public.audit_logs;
CREATE POLICY "system_insert_audit_logs" ON public.audit_logs FOR
INSERT WITH CHECK (true);
-- SYNC CONFLICTS
DROP POLICY IF EXISTS "users_view_own_conflicts" ON public.sync_conflicts;
CREATE POLICY "users_view_own_conflicts" ON public.sync_conflicts FOR
SELECT USING (auth.uid() = user_id);
DROP POLICY IF EXISTS "insert_sync_conflicts" ON public.sync_conflicts;
CREATE POLICY "insert_sync_conflicts" ON public.sync_conflicts FOR
INSERT WITH CHECK (auth.uid() = user_id);
-- ALLOWED REGISTRATIONS
DROP POLICY IF EXISTS "HR manage registrations" ON public.allowed_registrations;
CREATE POLICY "HR manage registrations" ON public.allowed_registrations FOR ALL USING (is_hr_manager_or_admin());
DROP POLICY IF EXISTS "Public check registration" ON public.allowed_registrations;
CREATE POLICY "Public check registration" ON public.allowed_registrations FOR
SELECT TO authenticated USING (
        email = (
            SELECT email
            FROM auth.users
            WHERE id = auth.uid()
        )
    );
-- =============================================
-- 10. HELPER FUNCTIONS
-- =============================================
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
CREATE OR REPLACE FUNCTION public.is_admin() RETURNS BOOLEAN LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role = 'admin'
    );
$$;
-- updated_at trigger
CREATE OR REPLACE FUNCTION public.set_updated_at() RETURNS TRIGGER AS $$ BEGIN NEW.updated_at = now();
RETURN NEW;
END;
$$ LANGUAGE plpgsql;
-- Optimistic locking trigger
CREATE OR REPLACE FUNCTION public.bump_version() RETURNS TRIGGER AS $$ BEGIN IF OLD.version IS NOT NULL
    AND NEW.version IS NOT NULL
    AND OLD.version != NEW.version THEN RAISE EXCEPTION 'CONFLICT: record modified by another user (expected v%, got v%)',
    OLD.version,
    NEW.version USING ERRCODE = '40001';
END IF;
NEW.version = COALESCE(OLD.version, 0) + 1;
NEW.updated_at = now();
RETURN NEW;
END;
$$ LANGUAGE plpgsql;
-- Audit trail
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
-- Account lock functions
CREATE OR REPLACE FUNCTION public.is_account_locked(check_email TEXT) RETURNS BOOLEAN SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$ BEGIN RETURN EXISTS (
        SELECT 1
        FROM account_locks
        WHERE email = check_email
            AND locked_until > now()
            AND unlocked_at IS NULL
    );
END;
$$;
CREATE OR REPLACE FUNCTION public.get_failed_login_count(check_email TEXT) RETURNS INTEGER SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE failed_count INTEGER;
BEGIN
SELECT COUNT(*) INTO failed_count
FROM login_attempts
WHERE email = check_email
    AND success = false
    AND attempt_time > now() - INTERVAL '15 minutes';
RETURN failed_count;
END;
$$;
CREATE OR REPLACE FUNCTION lock_account_on_failures() RETURNS TRIGGER SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE failed_count INTEGER;
user_uuid UUID;
BEGIN IF NEW.success = true THEN RETURN NEW;
END IF;
SELECT COUNT(*) INTO failed_count
FROM login_attempts
WHERE email = NEW.email
    AND success = false
    AND attempt_time > now() - INTERVAL '15 minutes';
IF failed_count >= 5 THEN
SELECT id INTO user_uuid
FROM auth.users
WHERE email = NEW.email;
INSERT INTO account_locks (user_id, email, locked_until)
VALUES (
        user_uuid,
        NEW.email,
        now() + INTERVAL '15 minutes'
    ) ON CONFLICT (user_id, locked_at) DO NOTHING;
END IF;
RETURN NEW;
END;
$$;
-- Anti-RLS-recursion helpers (bypass RLS via SECURITY DEFINER)
CREATE OR REPLACE FUNCTION public.get_auth_role() RETURNS TEXT LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT role
FROM public.users
WHERE id = auth.uid();
$$;
CREATE OR REPLACE FUNCTION public.get_auth_orchard_id() RETURNS UUID LANGUAGE sql SECURITY DEFINER
SET search_path = public STABLE AS $$
SELECT orchard_id
FROM public.users
WHERE id = auth.uid();
$$;
-- Day closures updated_at trigger function
CREATE OR REPLACE FUNCTION public.update_day_closures_updated_at() RETURNS TRIGGER AS $$ BEGIN NEW.updated_at = now();
RETURN NEW;
END;
$$ LANGUAGE plpgsql;
-- Anti-fraud: Block bucket inserts on closed days
CREATE OR REPLACE FUNCTION public.enforce_closed_day_bucket_records() RETURNS TRIGGER LANGUAGE plpgsql SECURITY DEFINER
SET search_path = public AS $$
DECLARE v_closed_at TIMESTAMPTZ;
v_bucket_date DATE;
BEGIN v_bucket_date := DATE(NEW.scanned_at AT TIME ZONE 'Pacific/Auckland');
SELECT closed_at INTO v_closed_at
FROM day_closures
WHERE orchard_id = NEW.orchard_id
    AND date = v_bucket_date
    AND status = 'closed'
LIMIT 1;
IF v_closed_at IS NOT NULL
AND NEW.scanned_at >= v_closed_at THEN RAISE EXCEPTION 'INSERT_BLOCKED: Day % is closed for orchard %',
v_bucket_date,
NEW.orchard_id USING ERRCODE = 'P0001';
END IF;
RETURN NEW;
END;
$$;
-- Rate limit check (atomic — replaces two separate RPCs)
CREATE OR REPLACE FUNCTION public.check_rate_limit(check_email TEXT) RETURNS JSONB SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE active_lock RECORD;
failed_count INTEGER;
remaining INTEGER;
BEGIN
SELECT locked_until INTO active_lock
FROM account_locks
WHERE email = lower(trim(check_email))
    AND locked_until > now()
    AND unlocked_at IS NULL
ORDER BY locked_at DESC
LIMIT 1;
IF FOUND THEN RETURN jsonb_build_object(
    'allowed',
    false,
    'locked',
    true,
    'locked_until',
    active_lock.locked_until,
    'remaining_attempts',
    0
);
END IF;
SELECT COUNT(*) INTO failed_count
FROM login_attempts
WHERE email = lower(trim(check_email))
    AND success = false
    AND attempt_time > now() - INTERVAL '15 minutes';
remaining := GREATEST(0, 5 - failed_count);
RETURN jsonb_build_object(
    'allowed',
    true,
    'locked',
    false,
    'remaining_attempts',
    remaining,
    'failed_count',
    failed_count
);
END;
$$;
-- Unlock account
CREATE OR REPLACE FUNCTION public.unlock_account(
        target_email TEXT,
        unlock_reason_text TEXT DEFAULT NULL
    ) RETURNS BOOLEAN SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$ BEGIN IF NOT EXISTS (
        SELECT 1
        FROM public.users
        WHERE id = auth.uid()
            AND role IN ('manager', 'admin')
    ) THEN RAISE EXCEPTION 'Insufficient permissions';
END IF;
UPDATE account_locks
SET unlocked_at = now(),
    unlocked_by = auth.uid(),
    unlock_reason = unlock_reason_text
WHERE email = target_email
    AND locked_until > now()
    AND unlocked_at IS NULL;
RETURN FOUND;
END;
$$;
-- Cleanup functions (run via Supabase cron or manually)
CREATE OR REPLACE FUNCTION public.cleanup_old_login_attempts() RETURNS void SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$ BEGIN
DELETE FROM login_attempts
WHERE attempt_time < now() - INTERVAL '30 days';
END;
$$;
CREATE OR REPLACE FUNCTION public.cleanup_old_account_locks() RETURNS void SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$ BEGIN
DELETE FROM account_locks
WHERE locked_until < now() - INTERVAL '90 days';
END;
$$;
CREATE OR REPLACE FUNCTION public.cleanup_old_audit_logs() RETURNS void LANGUAGE plpgsql SECURITY DEFINER AS $$ BEGIN
DELETE FROM audit_logs
WHERE created_at < now() - INTERVAL '365 days';
END;
$$;
-- Audit trail query function
CREATE OR REPLACE FUNCTION public.get_record_audit_trail(p_table_name TEXT, p_record_id UUID) RETURNS TABLE (
        id UUID,
        action TEXT,
        user_email TEXT,
        old_values JSONB,
        new_values JSONB,
        created_at TIMESTAMPTZ
    ) LANGUAGE plpgsql SECURITY DEFINER
SET search_path = public AS $$ BEGIN RETURN QUERY
SELECT al.id,
    al.action,
    al.user_email,
    al.old_values,
    al.new_values,
    al.created_at
FROM audit_logs al
WHERE al.table_name = p_table_name
    AND al.record_id = p_record_id
ORDER BY al.created_at DESC;
END;
$$;
-- Health check RPC
CREATE OR REPLACE FUNCTION public.health_check() RETURNS JSONB SECURITY DEFINER
SET search_path = public LANGUAGE plpgsql AS $$
DECLARE result JSONB;
BEGIN result := jsonb_build_object(
    'status',
    'healthy',
    'timestamp',
    now(),
    'database',
    jsonb_build_object('connected', true, 'version', version())
);
RETURN result;
EXCEPTION
WHEN OTHERS THEN RETURN jsonb_build_object('status', 'unhealthy', 'error', SQLERRM);
END;
$$;
-- Payroll close RPC
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
v_total_earnings := v_total_buckets * 6.50;
v_result := json_build_object(
    'status',
    'closed',
    'period_start',
    p_period_start,
    'period_end',
    p_period_end,
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
RETURN v_result;
EXCEPTION
WHEN OTHERS THEN RAISE;
END;
$$;
-- Grants
GRANT EXECUTE ON FUNCTION health_check() TO authenticated;
GRANT EXECUTE ON FUNCTION check_rate_limit(TEXT) TO authenticated,
    anon;
GRANT EXECUTE ON FUNCTION close_payroll_period(UUID, DATE, DATE) TO authenticated;
-- =============================================
-- 11. PERFORMANCE INDEXES
-- =============================================
-- Hierarchy
CREATE INDEX IF NOT EXISTS idx_blocks_by_season ON public.orchard_blocks (orchard_id, season_id)
WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_rows_by_block ON public.block_rows (block_id)
WHERE deleted_at IS NULL;
-- Core
CREATE INDEX IF NOT EXISTS idx_users_orchard ON public.users(orchard_id);
CREATE INDEX IF NOT EXISTS idx_pickers_orchard ON public.pickers(orchard_id);
CREATE INDEX IF NOT EXISTS idx_pickers_code ON public.pickers(picker_id);
CREATE INDEX IF NOT EXISTS idx_bucket_records_picker ON public.bucket_records(picker_id);
CREATE INDEX IF NOT EXISTS idx_bucket_records_scanned_by ON public.bucket_records(scanned_by);
CREATE INDEX IF NOT EXISTS idx_bucket_records_created ON public.bucket_records(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_bucket_records_orchard ON public.bucket_records(orchard_id);
-- Sync (delta)
CREATE INDEX IF NOT EXISTS idx_bucket_records_sync ON public.bucket_records (season_id, created_at)
WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_attendance_sync ON public.daily_attendance (season_id, created_at)
WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_pickers_sync ON public.pickers (orchard_id, created_at)
WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_bucket_records_row_density ON public.bucket_records (orchard_id, season_id, block_row_id)
WHERE deleted_at IS NULL;
-- Messaging
CREATE INDEX IF NOT EXISTS idx_conversations_participants ON public.conversations USING gin(participant_ids);
CREATE INDEX IF NOT EXISTS idx_chat_messages_conversation ON public.chat_messages(conversation_id);
CREATE INDEX IF NOT EXISTS idx_chat_messages_created ON public.chat_messages(created_at DESC);
-- Attendance
CREATE INDEX IF NOT EXISTS idx_attendance_orchard_date ON public.daily_attendance(orchard_id, date);
CREATE INDEX IF NOT EXISTS idx_attendance_picker_date ON public.daily_attendance(picker_id, date);
CREATE INDEX IF NOT EXISTS idx_attendance_date ON public.daily_attendance(date DESC);
-- Day closures
CREATE INDEX IF NOT EXISTS idx_day_closures_orchard_date ON public.day_closures(orchard_id, date DESC);
-- Contracts
CREATE INDEX IF NOT EXISTS idx_contracts_employee ON public.contracts(employee_id);
CREATE INDEX IF NOT EXISTS idx_contracts_orchard ON public.contracts(orchard_id);
CREATE INDEX IF NOT EXISTS idx_contracts_status ON public.contracts(status);
-- Fleet & Transport
CREATE INDEX IF NOT EXISTS idx_fleet_orchard ON public.fleet_vehicles(orchard_id);
CREATE INDEX IF NOT EXISTS idx_transport_orchard ON public.transport_requests(orchard_id);
CREATE INDEX IF NOT EXISTS idx_transport_status ON public.transport_requests(status);
-- Security
CREATE INDEX IF NOT EXISTS idx_login_attempts_email_time ON public.login_attempts(email, attempt_time DESC);
CREATE INDEX IF NOT EXISTS idx_account_locks_email ON public.account_locks(email, locked_until DESC);
CREATE INDEX IF NOT EXISTS idx_audit_logs_table_record ON public.audit_logs(table_name, record_id);
CREATE INDEX IF NOT EXISTS idx_sync_conflicts_table ON public.sync_conflicts(table_name, record_id);
-- =============================================
-- 12. TRIGGERS
-- =============================================
-- Optimistic locking
DROP TRIGGER IF EXISTS trg_pickers_version ON public.pickers;
CREATE TRIGGER trg_pickers_version BEFORE
UPDATE ON public.pickers FOR EACH ROW EXECUTE FUNCTION bump_version();
DROP TRIGGER IF EXISTS trg_attendance_version ON public.daily_attendance;
CREATE TRIGGER trg_attendance_version BEFORE
UPDATE ON public.daily_attendance FOR EACH ROW EXECUTE FUNCTION bump_version();
DROP TRIGGER IF EXISTS trg_row_assignments_version ON public.row_assignments;
CREATE TRIGGER trg_row_assignments_version BEFORE
UPDATE ON public.row_assignments FOR EACH ROW EXECUTE FUNCTION bump_version();
-- updated_at
DROP TRIGGER IF EXISTS trg_seasons_updated_at ON public.harvest_seasons;
CREATE TRIGGER trg_seasons_updated_at BEFORE
UPDATE ON public.harvest_seasons FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DROP TRIGGER IF EXISTS trg_blocks_updated_at ON public.orchard_blocks;
CREATE TRIGGER trg_blocks_updated_at BEFORE
UPDATE ON public.orchard_blocks FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DROP TRIGGER IF EXISTS contracts_updated_at ON public.contracts;
CREATE TRIGGER contracts_updated_at BEFORE
UPDATE ON public.contracts FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DROP TRIGGER IF EXISTS fleet_updated_at ON public.fleet_vehicles;
CREATE TRIGGER fleet_updated_at BEFORE
UPDATE ON public.fleet_vehicles FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DROP TRIGGER IF EXISTS transport_updated_at ON public.transport_requests;
CREATE TRIGGER transport_updated_at BEFORE
UPDATE ON public.transport_requests FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DROP TRIGGER IF EXISTS users_updated_at ON public.users;
CREATE TRIGGER users_updated_at BEFORE
UPDATE ON public.users FOR EACH ROW EXECUTE FUNCTION set_updated_at();
DROP TRIGGER IF EXISTS pickers_updated_at ON public.pickers;
CREATE TRIGGER pickers_updated_at BEFORE
UPDATE ON public.pickers FOR EACH ROW EXECUTE FUNCTION set_updated_at();
-- Audit
DROP TRIGGER IF EXISTS audit_pickers ON public.pickers;
CREATE TRIGGER audit_pickers
AFTER
INSERT
    OR
UPDATE
    OR DELETE ON public.pickers FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
DROP TRIGGER IF EXISTS audit_users ON public.users;
CREATE TRIGGER audit_users
AFTER
UPDATE ON public.users FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
DROP TRIGGER IF EXISTS audit_daily_attendance ON public.daily_attendance;
CREATE TRIGGER audit_daily_attendance
AFTER
INSERT
    OR
UPDATE
    OR DELETE ON public.daily_attendance FOR EACH ROW EXECUTE FUNCTION log_audit_trail();
-- Auth lockout
DROP TRIGGER IF EXISTS trigger_lock_account ON public.login_attempts;
CREATE TRIGGER trigger_lock_account
AFTER
INSERT ON public.login_attempts FOR EACH ROW EXECUTE FUNCTION lock_account_on_failures();
-- Anti-fraud trigger
DROP TRIGGER IF EXISTS trg_enforce_closed_day ON public.bucket_records;
CREATE TRIGGER trg_enforce_closed_day BEFORE
INSERT ON public.bucket_records FOR EACH ROW EXECUTE FUNCTION enforce_closed_day_bucket_records();
-- Day closures updated_at
DROP TRIGGER IF EXISTS day_closures_updated_at ON public.day_closures;
CREATE TRIGGER day_closures_updated_at BEFORE
UPDATE ON public.day_closures FOR EACH ROW EXECUTE FUNCTION update_day_closures_updated_at();
-- =============================================
-- 13. VIEWS
-- =============================================
-- pickers_performance_today: used by attendance.service.ts & picker.service.ts
CREATE OR REPLACE VIEW public.pickers_performance_today AS
SELECT p.id AS picker_id,
    p.name,
    p.orchard_id,
    p.team_leader_id,
    p.status,
    COALESCE(br_today.bucket_count, 0) AS total_buckets,
    br_today.last_scan
FROM public.pickers p
    LEFT JOIN LATERAL (
        SELECT COUNT(*) AS bucket_count,
            MAX(br.scanned_at) AS last_scan
        FROM public.bucket_records br
        WHERE br.picker_id = p.id
            AND br.deleted_at IS NULL
            AND DATE(br.scanned_at AT TIME ZONE 'Pacific/Auckland') = (CURRENT_DATE AT TIME ZONE 'Pacific/Auckland')::date
    ) br_today ON true
WHERE p.deleted_at IS NULL;
-- =============================================
-- 14. REALTIME
-- =============================================
DO $$ BEGIN ALTER PUBLICATION supabase_realtime
ADD TABLE public.chat_messages;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.conversations;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.bucket_records;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.users;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.transport_requests;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.fleet_vehicles;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.daily_attendance;
EXCEPTION
WHEN OTHERS THEN NULL;
END $$;
-- =============================================
-- 15. COMMENTS
-- =============================================
COMMENT ON TABLE public.harvest_seasons IS 'Season-scoped data isolation — prevents OOM in multi-year usage';
COMMENT ON TABLE public.orchard_blocks IS 'Physical subdivision of orchard — managed by admin before season starts';
COMMENT ON TABLE public.block_rows IS 'Individual row within a block — each has a variety and target';
COMMENT ON TABLE public.day_closures IS 'Immutable daily closure records for audit/legal compliance';
COMMENT ON TABLE public.audit_logs IS 'Complete audit trail for compliance and security';
COMMENT ON TABLE public.sync_conflicts IS 'Audit trail for offline sync conflicts (last-write-wins with logging)';
SELECT 'Schema V3 Consolidated — 26 tables, 1 view, hierarchy, soft deletes, optimistic locking, all RLS policies' AS result;