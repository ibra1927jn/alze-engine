-- FIX: Permisos de Manager para Gestionar Huertos y Usuarios

-- 1. HUERTOS (ORCHARDS)
-- Asegurar que los usuarios logueados puedan leer la tabla orchards
-- Usamos una política simple para evitar recursión.
ALTER TABLE public.orchards ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS "Enable read access for authenticated users" ON public.orchards;
-- Permitir a cualquiera autenticado leer la lista de huertos (necesario para selects/nombres)
CREATE POLICY "Enable read access for authenticated users" ON public.orchards
FOR SELECT TO authenticated USING (true);


-- 2. USUARIOS (USERS) - UPDATE
-- Los Managers necesitan poder actualizar el 'orchard_id' de otros usuarios para asignarlos.
DROP POLICY IF EXISTS "Managers can update users" ON public.users;

CREATE POLICY "Managers can update users" ON public.users
FOR UPDATE USING (
  -- Solo Managers y Admins pueden actualizar
  get_auth_role() IN ('manager', 'admin')
);


-- 3. TRABAJADORES (PICKERS) - ALL
-- Los Managers gestionan la tabla pickers (insertar, editar, borrar).
ALTER TABLE public.pickers ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS "Managers manage pickers" ON public.pickers;

CREATE POLICY "Managers manage pickers" ON public.pickers
FOR ALL USING (
  -- Managers y Admins tienen control total
  get_auth_role() IN ('manager', 'admin')
);

-- Política básica de lectura para el resto (si no son managers)
-- Por ejemplo, Team Leaders viendo su propio equipo
DROP POLICY IF EXISTS "Team Leaders view own team" ON public.pickers;
CREATE POLICY "Team Leaders view own team" ON public.pickers
FOR SELECT USING (
  auth.uid() = team_leader_id 
  OR 
  orchard_id = get_auth_orchard_id()
);
