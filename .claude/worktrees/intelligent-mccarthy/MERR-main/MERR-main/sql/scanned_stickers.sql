-- =============================================
-- SCANNED STICKERS TABLE
-- =============================================
-- Tabla para rastrear stickers/bins escaneados y prevenir duplicados
-- El código del sticker contiene el ID del picker al inicio
-- Ejemplo: 2662200498 donde 26220 es el picker_id

CREATE TABLE IF NOT EXISTS scanned_stickers (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    sticker_code VARCHAR(50) NOT NULL UNIQUE,      -- Código completo del sticker (ej: 2662200498)
    picker_id VARCHAR(20),                          -- ID del picker extraído del código (ej: 26220)
    bin_id VARCHAR(50),                             -- ID del bin donde se escaneó
    scanned_by UUID REFERENCES users(id),           -- Usuario que escaneó (runner)
    team_leader_id UUID REFERENCES users(id),       -- Team leader del departamento
    orchard_id UUID REFERENCES orchards(id),        -- Huerta donde se escaneó
    scanned_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Índice para búsquedas rápidas por código
CREATE INDEX IF NOT EXISTS idx_scanned_stickers_code ON scanned_stickers(sticker_code);

-- Índice para búsquedas por picker
CREATE INDEX IF NOT EXISTS idx_scanned_stickers_picker ON scanned_stickers(picker_id);

-- Índice para búsquedas por fecha
CREATE INDEX IF NOT EXISTS idx_scanned_stickers_date ON scanned_stickers(scanned_at);

-- Índice para team leader
CREATE INDEX IF NOT EXISTS idx_scanned_stickers_team_leader ON scanned_stickers(team_leader_id);

-- RLS (Row Level Security) policies
ALTER TABLE scanned_stickers ENABLE ROW LEVEL SECURITY;

-- Política para permitir SELECT a usuarios autenticados
CREATE POLICY "Users can view scanned stickers" ON scanned_stickers
    FOR SELECT USING (auth.role() = 'authenticated');

-- Política para permitir INSERT a usuarios autenticados
CREATE POLICY "Users can insert scanned stickers" ON scanned_stickers
    FOR INSERT WITH CHECK (auth.role() = 'authenticated');
