-- fix_runtime_errors.sql
-- OBJECTIVE: Fix missing view enabling 404 errors, and add missing settings for default orchard to fix 406 errors.

-- 1. Create missing view: pickers_performance_today
CREATE OR REPLACE VIEW pickers_performance_today AS
WITH today_scans AS (
    SELECT 
        picker_id,
        scanned_at,
        orchard_id
    FROM 
        bucket_records
    WHERE 
        scanned_at >= CURRENT_DATE
),
scan_stats AS (
    SELECT 
        picker_id,
        orchard_id,
        COUNT(*) as total_buckets,
        MIN(scanned_at) as first_scan,
        MAX(scanned_at) as last_scan
    FROM 
        today_scans
    GROUP BY 
        picker_id, orchard_id
)
SELECT 
    s.picker_id,
    p.full_name as picker_name,
    p.harness_number as harness_id,
    p.team_leader_id as team_id,
    s.orchard_id,
    s.total_buckets,
    s.first_scan,
    s.last_scan,
    ROUND(
        (EXTRACT(EPOCH FROM (s.last_scan - s.first_scan)) / 3600.0)::numeric + 0.5, 
        2
    ) as hours_worked,
    ROUND(
        (s.total_buckets / ((EXTRACT(EPOCH FROM (s.last_scan - s.first_scan)) / 3600.0) + 0.5))::numeric, 
        2
    ) as buckets_per_hour,
    CASE 
        WHEN (s.total_buckets / ((EXTRACT(EPOCH FROM (s.last_scan - s.first_scan)) / 3600.0) + 0.5)) >= 3.6 THEN 'safe'
        WHEN (s.total_buckets / ((EXTRACT(EPOCH FROM (s.last_scan - s.first_scan)) / 3600.0) + 0.5)) >= 2.8 THEN 'warning'
        ELSE 'critical'
    END as status_shield
FROM 
    scan_stats s
JOIN 
    pickers p ON s.picker_id = p.id;

GRANT SELECT ON pickers_performance_today TO authenticated;

-- 2. Insert Default Harvest Settings for the Default Orchard (ID: a000...001)
-- This fixes the 406 Not Acceptable error when the app falls back to this orchard.
INSERT INTO harvest_settings (orchard_id, min_wage_rate, piece_rate, min_buckets_per_hour, target_tons)
VALUES (
    'a0000000-0000-0000-0000-000000000001', 
    23.50,  -- Min Wage
    6.50,   -- Piece Rate
    3.6,    -- Min Buckets/Hr
    40.0    -- Target Tons
)
ON CONFLICT (orchard_id) DO NOTHING;

-- 3. Validation
SELECT * FROM harvest_settings WHERE orchard_id = 'a0000000-0000-0000-0000-000000000001';
SELECT count(*) as view_rows FROM pickers_performance_today;
