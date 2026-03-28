-- QC Inspections Table
-- Run this in Supabase SQL Editor to enable the QC Inspector functionality.
-- This stores individual fruit quality grade inspections.
CREATE TABLE IF NOT EXISTS qc_inspections (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    orchard_id UUID NOT NULL REFERENCES orchards(id) ON DELETE CASCADE,
    picker_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    inspector_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    grade TEXT NOT NULL CHECK (grade IN ('A', 'B', 'C', 'reject')),
    notes TEXT,
    photo_url TEXT,
    created_at TIMESTAMPTZ DEFAULT now()
);
-- Index for common queries
CREATE INDEX IF NOT EXISTS idx_qc_inspections_orchard_date ON qc_inspections (orchard_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_qc_inspections_picker ON qc_inspections (picker_id, created_at DESC);
-- RLS (Row Level Security)
ALTER TABLE qc_inspections ENABLE ROW LEVEL SECURITY;
-- Policy: QC Inspectors and Managers can read all inspections for their orchard
CREATE POLICY "Users can read inspections for their orchard" ON qc_inspections FOR
SELECT USING (
        orchard_id IN (
            SELECT orchard_id
            FROM users
            WHERE id = auth.uid()
        )
    );
-- Policy: QC Inspectors can insert inspections
CREATE POLICY "QC inspectors can create inspections" ON qc_inspections FOR
INSERT WITH CHECK (inspector_id = auth.uid());