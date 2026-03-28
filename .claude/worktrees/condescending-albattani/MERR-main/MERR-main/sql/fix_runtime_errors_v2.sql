-- fix_runtime_errors_v2.sql
-- 1. Crear Vista de Rendimiento (Fix 404)
CREATE OR REPLACE VIEW public.pickers_performance_today AS
  SELECT 
    p.id as picker_id,
    p.orchard_id,
    p.team_leader_id, -- CRITICAL: Required for Dashboard grouping
    COUNT(b.id) as total_buckets,
    -- Simulación de horas trabajadas (ajustar según lógica real)
    -- Evitar división por cero si no hay buckets
    CASE 
        WHEN MIN(b.scanned_at) IS NULL THEN 0
        ELSE EXTRACT(EPOCH FROM (NOW() - MIN(b.scanned_at))) / 3600 
    END as hours_worked
  FROM public.pickers p
  LEFT JOIN public.bucket_records b ON p.id = b.picker_id
  WHERE b.scanned_at >= CURRENT_DATE
  GROUP BY p.id, p.orchard_id, p.team_leader_id;

-- Grant access
GRANT SELECT ON public.pickers_performance_today TO authenticated;

-- 2. Configuración para Orcha por Defecto (Fix 406)
INSERT INTO public.harvest_settings (orchard_id, min_wage_rate, piece_rate, target_tons)
VALUES ('a0000000-0000-0000-0000-000000000001', 23.50, 6.50, 40.0)
ON CONFLICT (orchard_id) DO NOTHING;

-- 3. Validation
SELECT * FROM pickers_performance_today LIMIT 5;
SELECT * FROM harvest_settings WHERE orchard_id = 'a0000000-0000-0000-0000-000000000001';
