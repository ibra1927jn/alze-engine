-- 1. Asegurar que orchard_id pueda ser NULL (Permitir trabajadores "En Banca")
ALTER TABLE public.pickers ALTER COLUMN orchard_id DROP NOT NULL;

-- 2. ELIMINAR la política restrictiva actual
DROP POLICY IF EXISTS "Read pickers" ON public.pickers;

-- 3. CREAR la nueva política "Ver mi Equipo Completo"
-- Esta regla dice: "Muéstrame el trabajador SI es mi empleado (soy su jefe) O SI está trabajando en mi huerto actual".
CREATE POLICY "Read pickers" ON public.pickers
FOR SELECT USING (
    (auth.uid() = team_leader_id) -- Muestra mi Roster histórico (estén donde estén)
    OR 
    (orchard_id IS NOT NULL AND orchard_id::text = current_setting('app.current_orchard_id', true)) -- (Opcional) Muestra gente activa en mi ubicación si usas variables de sesión, simplificado para este caso:
    -- O simplemente confiar en que si eres TL, ves a tu equipo.
);

-- Nota: La política original sugerida por el usuario usa get_my_orchard_id() que puede no existir.
-- Vamos a usar una política más robusta basada en el ID del usuario:

DROP POLICY IF EXISTS "Read pickers" ON public.pickers;
CREATE POLICY "Read pickers" ON public.pickers
FOR ALL USING (
    auth.uid() = team_leader_id OR
    exists (
      select 1 from public.users where id = auth.uid() and role = 'manager'
    )
);


-- 4. Permitir crear trabajadores sin huerto (para Onboarding previo)
DROP POLICY IF EXISTS "Manage pickers" ON public.pickers;
CREATE POLICY "Manage pickers" ON public.pickers
FOR ALL USING (
    auth.uid() = team_leader_id OR
    exists (
      select 1 from public.users where id = auth.uid() and role = 'manager'
    )
);
