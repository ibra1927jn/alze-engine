-- 1. Eliminar la política restrictiva actual de usuarios
DROP POLICY IF EXISTS "Read orchard members" ON public.users;

-- 2. Crear la nueva política de Acceso Jerárquico
-- Esta regla permite SELECT si:
-- A) Es el propio usuario (siempre).
-- B) El usuario que consulta es MANAGER o ADMIN (ven a todos).
-- C) El usuario consultado está en el mismo huerto que el que consulta (visibilidad de compañeros).

CREATE POLICY "Global Directory Access for Managers" ON public.users
FOR SELECT USING (
    auth.uid() = id -- A) Ver mi propio perfil
    OR
    EXISTS ( -- B) Soy Manager/Admin -> Veo todo
        SELECT 1 FROM public.users 
        WHERE id = auth.uid() 
        AND role IN ('manager', 'admin')
    )
    OR
    orchard_id = ( -- C) Compañeros de huerto -> Veo a mis pares
        SELECT orchard_id FROM public.users WHERE id = auth.uid()
    )
);
