-- ============================================
-- FASE 5: Day Closures & Immutability
-- ============================================

-- 1. Crear tabla day_closures
CREATE TABLE IF NOT EXISTS day_closures (
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  orchard_id UUID NOT NULL REFERENCES orchards(id) ON DELETE CASCADE,
  date DATE NOT NULL,
  status TEXT NOT NULL CHECK (status IN ('open', 'closed')),
  closed_by UUID REFERENCES users(id),
  closed_at TIMESTAMPTZ,
  
  -- Snapshot financiero (calculado por Edge Function)
  total_buckets INTEGER NOT NULL DEFAULT 0,
  total_cost DECIMAL(10,2) NOT NULL DEFAULT 0.00,
  total_hours DECIMAL(8,2),
  wage_violations INTEGER DEFAULT 0,
  
  -- Metadata
  created_at TIMESTAMPTZ DEFAULT NOW(),
  updated_at TIMESTAMPTZ DEFAULT NOW(),
  
  -- Constraint: Un solo cierre por día por orchard
  UNIQUE(orchard_id, date)
);

-- Índices para performance
CREATE INDEX idx_day_closures_orchard_date ON day_closures(orchard_id, date DESC);
CREATE INDEX idx_day_closures_status ON day_closures(status);
CREATE INDEX idx_day_closures_closed_at ON day_closures(closed_at DESC);

-- Trigger para updated_at
CREATE OR REPLACE FUNCTION update_day_closures_updated_at()
RETURNS TRIGGER AS $$
BEGIN
  NEW.updated_at = NOW();
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER day_closures_updated_at
  BEFORE UPDATE ON day_closures
  FOR EACH ROW
  EXECUTE FUNCTION update_day_closures_updated_at();

-- 2. RLS Policies para Inmutabilidad de bucket_events

-- RLS: No INSERT en días cerrados
CREATE POLICY "no_insert_on_closed_days"
ON bucket_events
FOR INSERT
WITH CHECK (
  NOT EXISTS (
    SELECT 1 FROM day_closures
    WHERE day_closures.orchard_id = bucket_events.orchard_id
    AND day_closures.date = DATE(bucket_events.recorded_at)
    AND day_closures.status = 'closed'
  )
);

-- RLS: No UPDATE en días cerrados
CREATE POLICY "no_update_on_closed_days"
ON bucket_events
FOR UPDATE
USING (
  NOT EXISTS (
    SELECT 1 FROM day_closures
    WHERE day_closures.orchard_id = bucket_events.orchard_id
    AND day_closures.date = DATE(bucket_events.recorded_at)
    AND day_closures.status = 'closed'
  )
);

-- RLS: No DELETE en días cerrados
CREATE POLICY "no_delete_on_closed_days"
ON bucket_events
FOR DELETE
USING (
  NOT EXISTS (
    SELECT 1 FROM day_closures
    WHERE day_closures.orchard_id = bucket_events.orchard_id
    AND day_closures.date = DATE(bucket_events.recorded_at)
    AND day_closures.status = 'closed'
  )
);

-- 3. RLS Policies para day_closures

-- Permitir SELECT a usuarios autenticados
CREATE POLICY "authenticated_select_day_closures"
ON day_closures
FOR SELECT
USING (auth.role() = 'authenticated');

-- Permitir INSERT a usuarios autenticados (managers/team leaders)
-- Nota: Se puede refinar después agregando verificación de role si es necesario
CREATE POLICY "authenticated_insert_day_closures"
ON day_closures
FOR INSERT
WITH CHECK (
  auth.role() = 'authenticated'
);

-- No se permite UPDATE ni DELETE (inmutabilidad total)
-- Si se necesita corrección, debe hacerse manualmente en Supabase por admin

-- 4. Comentarios para documentación
COMMENT ON TABLE day_closures IS 'Registro inmutable de cierres de jornada para auditoría legal';
COMMENT ON COLUMN day_closures.status IS 'Estado del día: open (default) o closed (inmutable)';
COMMENT ON COLUMN day_closures.total_cost IS 'Costo total incluyendo wage top-ups calculado por Edge Function';
COMMENT ON COLUMN day_closures.wage_violations IS 'Número de trabajadores que requirieron top-up para alcanzar salario mínimo';
