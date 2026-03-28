-- =============================================
-- FIX 403 ERROR ON BUCKET_RECORDS
-- Issue: Missing row_number and quality_grade columns
-- =============================================

-- Add missing columns to bucket_records table
DO $$
BEGIN
    -- Add row_number column if it doesn't exist
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns 
        WHERE table_name='bucket_records' AND column_name='row_number'
    ) THEN
        ALTER TABLE public.bucket_records ADD COLUMN row_number INTEGER;
        RAISE NOTICE 'Added row_number column to bucket_records';
    ELSE
        RAISE NOTICE 'row_number column already exists';
    END IF;

    -- Add quality_grade column if it doesn't exist
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns 
        WHERE table_name='bucket_records' AND column_name='quality_grade'
    ) THEN
        ALTER TABLE public.bucket_records ADD COLUMN quality_grade TEXT;
        RAISE NOTICE 'Added quality_grade column to bucket_records';
    ELSE
        RAISE NOTICE 'quality_grade column already exists';
    END IF;
END $$;

-- Verification: Show bucket_records structure
SELECT 
    column_name, 
    data_type, 
    is_nullable
FROM information_schema.columns
WHERE table_name = 'bucket_records'
ORDER BY ordinal_position;

SELECT '✅ Fix applied successfully. You can now scan buckets!' as status;
