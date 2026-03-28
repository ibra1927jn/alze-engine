-- =============================================
-- SMART HOURS VIEW: pickers_performance_today
-- =============================================
-- This view calculates real-time performance metrics for pickers based on today's bucket scans.
-- It infers "Hours Worked" from the first and last scan of the day + a 30-min buffer.

CREATE OR REPLACE VIEW pickers_performance_today AS
WITH today_scans AS (
    -- 1. Filter scans for the current day (Server Time)
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
    -- 2. Aggregate raw stats per picker
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
    p.harness_number as harness_id, -- Corregido: pickers usa harness_number
    p.team_leader_id as team_id,    -- Corregido: pickers usa team_leader_id
    s.orchard_id,
    s.total_buckets,
    s.first_scan,
    s.last_scan,
    -- 3. Calculate Hours: (Last - First) + 30 min buffer. Minimum 0.5 hours.
    ROUND(
        (EXTRACT(EPOCH FROM (s.last_scan - s.first_scan)) / 3600.0)::numeric + 0.5, 
        2
    ) as hours_worked,
    -- 4. Calculate Rate: Buckets / Hours
    ROUND(
        (s.total_buckets / ((EXTRACT(EPOCH FROM (s.last_scan - s.first_scan)) / 3600.0) + 0.5))::numeric, 
        2
    ) as buckets_per_hour,
    -- 5. Wage Shield Status
    CASE 
        WHEN (s.total_buckets / ((EXTRACT(EPOCH FROM (s.last_scan - s.first_scan)) / 3600.0) + 0.5)) >= 3.6 THEN 'safe'
        WHEN (s.total_buckets / ((EXTRACT(EPOCH FROM (s.last_scan - s.first_scan)) / 3600.0) + 0.5)) >= 2.8 THEN 'warning'
        ELSE 'critical'
    END as status_shield
FROM 
    scan_stats s
JOIN 
    pickers p ON s.picker_id = p.id; -- Corregido: JOIN con pickers

-- Grant access to authenticated users
GRANT SELECT ON pickers_performance_today TO authenticated;

-- Comment
COMMENT ON VIEW pickers_performance_today IS 'Real-time performance metrics for today, including inferred hours worked.';
