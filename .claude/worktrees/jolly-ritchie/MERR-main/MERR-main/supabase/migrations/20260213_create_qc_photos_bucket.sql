-- Create qc-photos storage bucket for QC inspection evidence
-- Run this in the Supabase SQL Editor
-- 1. Create the bucket (public so photos can be viewed via URL)
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
        -- 5MB limit
        ARRAY ['image/webp', 'image/jpeg', 'image/png']::text []
    ) ON CONFLICT (id) DO NOTHING;
-- 2. Drop existing policies if any (idempotent)
DROP POLICY IF EXISTS "Authenticated users can upload QC photos" ON storage.objects;
DROP POLICY IF EXISTS "Public can view QC photos" ON storage.objects;
DROP POLICY IF EXISTS "Users can delete own QC photos" ON storage.objects;
-- 3. Allow authenticated users to upload photos
CREATE POLICY "Authenticated users can upload QC photos" ON storage.objects FOR
INSERT TO authenticated WITH CHECK (bucket_id = 'qc-photos');
-- 4. Allow public read access (for displaying thumbnails)
CREATE POLICY "Public can view QC photos" ON storage.objects FOR
SELECT TO public USING (bucket_id = 'qc-photos');
-- 5. Allow authenticated users to delete their own photos
CREATE POLICY "Users can delete own QC photos" ON storage.objects FOR DELETE TO authenticated USING (bucket_id = 'qc-photos');