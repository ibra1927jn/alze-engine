-- =========================================
-- HARVESTPRO: RESET COMPLETO DE PERSONAL
-- Ejecutar en Supabase SQL Editor
-- =========================================

-- ⚠️ CUIDADO: Este script elimina TODOS los datos de operaciones

-- 1. Eliminar bucket_records (referencia a pickers)
DELETE FROM bucket_records;

-- 2. Limpiar attendance
DELETE FROM daily_attendance;

-- 3. Eliminar TODOS los pickers
DELETE FROM pickers;

-- 4. Desasociar usuarios del orchard (no elimina cuentas)
UPDATE users 
SET orchard_id = NULL 
WHERE role IN ('team_leader', 'runner', 'picker');

-- 5. Verificar que está limpio
SELECT 'pickers' as tabla, COUNT(*) as registros FROM pickers
UNION ALL
SELECT 'daily_attendance', COUNT(*) FROM daily_attendance
UNION ALL
SELECT 'bucket_records', COUNT(*) FROM bucket_records;

-- =========================================
-- RESULTADO ESPERADO:
-- pickers | 0
-- daily_attendance | 0
-- bucket_records | 0
-- =========================================
