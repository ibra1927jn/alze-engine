-- pilot_setup_cooper_lane.sql
-- SETUP SCRIPT FOR COOPER LANE PILOT (CLEAN SLATE)
-- Generates Orchard and Default Settings. 
-- DOES NOT create users/pickers (Manager must do this manually).

-- 1. Create Orchard (if not exists)
INSERT INTO orchards (id, name, location)
VALUES ('3492576b-8422-4a00-998f-4efc96434407', 'MP3 - Cooper Lane', 'Cooper Lane, Cromwell')
ON CONFLICT (id) DO NOTHING;

-- 2. Create Default Harvest Settings for this Orchard
INSERT INTO harvest_settings (orchard_id, min_wage_rate, piece_rate, min_buckets_per_hour, target_tons)
VALUES (
    '3492576b-8422-4a00-998f-4efc96434407', 
    23.50,  -- Min Wage
    6.50,   -- Piece Rate (User Request)
    3.6,    -- Min Buckets/Hr (derived)
    40.0    -- Target Tons (User Request)
)
ON CONFLICT (orchard_id) DO UPDATE SET
    piece_rate = 6.50,
    target_tons = 40.0;


-- 3. Validation Output
SELECT * FROM orchards WHERE id = '3492576b-8422-4a00-998f-4efc96434407';
SELECT * FROM harvest_settings WHERE orchard_id = '3492576b-8422-4a00-998f-4efc96434407';
