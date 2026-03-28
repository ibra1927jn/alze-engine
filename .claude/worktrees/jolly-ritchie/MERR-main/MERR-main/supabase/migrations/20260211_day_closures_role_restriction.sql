-- =============================================
-- RLS: Restrict day_closures INSERT to managers only
-- Previously any authenticated user could close a day
-- =============================================

-- Drop the overly permissive policy
DROP POLICY IF EXISTS "authenticated_insert_day_closures" ON day_closures;

-- Create manager-only insert policy
-- Checks that the inserting user's ID matches a picker with role 'manager'
CREATE POLICY "manager_only_insert_day_closures"
ON day_closures
FOR INSERT
WITH CHECK (
    EXISTS (
        SELECT 1 FROM pickers
        WHERE pickers.user_id = auth.uid()
        AND pickers.role = 'manager'
    )
);

COMMENT ON POLICY "manager_only_insert_day_closures" ON day_closures IS
    'Only users with manager role in pickers table can close a day. Prevents pickers/runners from accidentally or maliciously closing days.';
