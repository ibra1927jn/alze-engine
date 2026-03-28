-- =============================================
-- PHASE 2 MIGRATION: contracts, fleet_vehicles, transport_requests
-- Date: 2026-02-13
-- Prerequisites: schema_v1_consolidated.sql applied
--
-- CONFLICT RESOLUTION STRATEGY:
--   All write operations from the client use syncService.addToQueue()
--   which applies LAST-WRITE-WINS semantics. This is a deliberate
--   architectural decision for this scale. If two coordinators assign
--   the same vehicle offline, the last sync wins. The updated_at
--   column is tracked for future optimistic-locking if needed.
-- =============================================
-- =============================================
-- 1. HELPER FUNCTION: Role check for HR/Manager/Admin
-- =============================================
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
-- =============================================
-- 2. CONTRACTS TABLE
-- =============================================
-- Decision: employee_id FK → users(id) with ON DELETE RESTRICT
-- Rationale: You should NOT be able to delete an employee who has
-- contracts (active or historical). Deactivate (is_active=false) instead.
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
    -- Business rule: end_date must be after start_date (if set)
    CONSTRAINT contracts_date_range CHECK (
        end_date IS NULL
        OR end_date >= start_date
    ),
    -- Unique active contract per employee (no overlaps)
    CONSTRAINT contracts_unique_active UNIQUE (employee_id, status) -- NOTE: This prevents 2 "active" contracts for same employee.
    -- Partial unique index below is more precise, but this is simpler.
);
-- Drop the overly-strict unique and use a partial unique index instead
ALTER TABLE public.contracts DROP CONSTRAINT IF EXISTS contracts_unique_active;
CREATE UNIQUE INDEX IF NOT EXISTS idx_contracts_one_active_per_employee ON public.contracts (employee_id)
WHERE status = 'active';
-- =============================================
-- 3. FLEET VEHICLES TABLE
-- =============================================
CREATE TABLE IF NOT EXISTS public.fleet_vehicles (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    orchard_id UUID NOT NULL REFERENCES public.orchards(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    -- e.g. 'T-001'
    registration TEXT,
    -- NZ plate number
    zone TEXT,
    -- Current zone assignment
    driver_id UUID REFERENCES public.users(id) ON DELETE
    SET NULL,
        driver_name TEXT,
        -- Denormalized for quick display
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
        -- Warrant of Fitness (NZ-specific)
        cof_expiry DATE,
        -- Certificate of Fitness (heavy vehicles)
        created_at TIMESTAMPTZ DEFAULT now(),
        updated_at TIMESTAMPTZ DEFAULT now()
);
-- =============================================
-- 4. TRANSPORT REQUESTS TABLE
-- =============================================
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
-- =============================================
-- 5. ROW LEVEL SECURITY
-- =============================================
ALTER TABLE public.contracts ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.fleet_vehicles ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.transport_requests ENABLE ROW LEVEL SECURITY;
-- 5.1 CONTRACTS POLICIES
-- HR Admin, Manager, Admin can read all contracts in their orchard
DROP POLICY IF EXISTS "HR read contracts" ON public.contracts;
CREATE POLICY "HR read contracts" ON public.contracts FOR
SELECT USING (
        orchard_id = get_my_orchard_id()
        AND is_hr_manager_or_admin()
    );
-- Employees can read their own contracts
DROP POLICY IF EXISTS "Employee read own contracts" ON public.contracts;
CREATE POLICY "Employee read own contracts" ON public.contracts FOR
SELECT USING (employee_id = auth.uid());
-- HR Admin, Manager, Admin can create/update contracts
DROP POLICY IF EXISTS "HR manage contracts" ON public.contracts;
CREATE POLICY "HR manage contracts" ON public.contracts FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_hr_manager_or_admin()
);
-- 5.2 FLEET VEHICLES POLICIES
-- Same orchard can read fleet
DROP POLICY IF EXISTS "Read fleet vehicles" ON public.fleet_vehicles;
CREATE POLICY "Read fleet vehicles" ON public.fleet_vehicles FOR
SELECT USING (orchard_id = get_my_orchard_id());
-- Logistics/Manager/Admin can manage fleet
DROP POLICY IF EXISTS "Manage fleet vehicles" ON public.fleet_vehicles;
CREATE POLICY "Manage fleet vehicles" ON public.fleet_vehicles FOR ALL USING (
    orchard_id = get_my_orchard_id()
    AND is_logistics_or_manager()
);
-- 5.3 TRANSPORT REQUESTS POLICIES
-- Same orchard can read all requests
DROP POLICY IF EXISTS "Read transport requests" ON public.transport_requests;
CREATE POLICY "Read transport requests" ON public.transport_requests FOR
SELECT USING (orchard_id = get_my_orchard_id());
-- Anyone in orchard can create a request (team leaders request pickups)
DROP POLICY IF EXISTS "Create transport requests" ON public.transport_requests;
CREATE POLICY "Create transport requests" ON public.transport_requests FOR
INSERT WITH CHECK (
        orchard_id = get_my_orchard_id()
        AND requested_by = auth.uid()
    );
-- Logistics/Manager can update requests (assign, complete, cancel)
DROP POLICY IF EXISTS "Manage transport requests" ON public.transport_requests;
CREATE POLICY "Manage transport requests" ON public.transport_requests FOR
UPDATE USING (
        orchard_id = get_my_orchard_id()
        AND is_logistics_or_manager()
    );
-- =============================================
-- 6. PERFORMANCE INDEXES
-- =============================================
-- Contracts
CREATE INDEX IF NOT EXISTS idx_contracts_employee ON public.contracts(employee_id);
CREATE INDEX IF NOT EXISTS idx_contracts_orchard ON public.contracts(orchard_id);
CREATE INDEX IF NOT EXISTS idx_contracts_status ON public.contracts(status);
CREATE INDEX IF NOT EXISTS idx_contracts_end_date ON public.contracts(end_date)
WHERE end_date IS NOT NULL;
-- Fleet
CREATE INDEX IF NOT EXISTS idx_fleet_orchard ON public.fleet_vehicles(orchard_id);
CREATE INDEX IF NOT EXISTS idx_fleet_status ON public.fleet_vehicles(status);
CREATE INDEX IF NOT EXISTS idx_fleet_driver ON public.fleet_vehicles(driver_id)
WHERE driver_id IS NOT NULL;
-- Transport Requests
CREATE INDEX IF NOT EXISTS idx_transport_orchard ON public.transport_requests(orchard_id);
CREATE INDEX IF NOT EXISTS idx_transport_status ON public.transport_requests(status);
CREATE INDEX IF NOT EXISTS idx_transport_priority ON public.transport_requests(priority, created_at DESC)
WHERE status IN ('pending', 'assigned');
CREATE INDEX IF NOT EXISTS idx_transport_created ON public.transport_requests(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_transport_vehicle ON public.transport_requests(assigned_vehicle)
WHERE assigned_vehicle IS NOT NULL;
-- =============================================
-- 7. UPDATED_AT TRIGGER (shared across all 3 tables)
-- =============================================
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
-- =============================================
-- 8. REALTIME (transport_requests for live dispatch)
-- =============================================
DO $$ BEGIN ALTER PUBLICATION supabase_realtime
ADD TABLE public.transport_requests;
ALTER PUBLICATION supabase_realtime
ADD TABLE public.fleet_vehicles;
EXCEPTION
WHEN OTHERS THEN NULL;
END $$;
SELECT 'Phase 2 Migration Applied Successfully' as result;