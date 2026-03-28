-- FIX: Evitar recursión infinita en RLS usando una función SECURITY DEFINER

-- 1. Crear función segura para leer el rol del usuario actual
-- SECURITY DEFINER hace que la función se ejecute con los permisos del creador (admin),
-- saltándose las políticas RLS de la tabla users al leer.

CREATE OR REPLACE FUNCTION public.get_auth_role()
RETURNS text
LANGUAGE sql
SECURITY DEFINER
SET search_path = public
AS $$
  SELECT role FROM public.users WHERE id = auth.uid();
$$;

CREATE OR REPLACE FUNCTION public.get_auth_orchard_id()
RETURNS uuid
LANGUAGE sql
SECURITY DEFINER
SET search_path = public
AS $$
  SELECT orchard_id FROM public.users WHERE id = auth.uid();
$$;

-- 2. Recrear la política usando las funciones seguras

DROP POLICY IF EXISTS "Global Directory Access for Managers" ON public.users;
DROP POLICY IF EXISTS "Read orchard members" ON public.users; -- Asegurar que la vieja no exista

CREATE POLICY "Global Directory Access for Managers" ON public.users
FOR SELECT USING (
    auth.uid() = id -- A) Ver mi propio perfil (siempre permitido)
    OR
    get_auth_role() IN ('manager', 'admin') -- B) Managers ven todo (usando función segura)
    OR
    orchard_id = get_auth_orchard_id() -- C) Compañeros de huerto (usando función segura)
);
