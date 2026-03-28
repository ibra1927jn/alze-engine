-- Migration: Add all 8 user roles
-- Date: 2026-02-12 (Updated 2026-02-13)
-- Description: Extends the user role options in the users table
--              to support all 8 application roles.
DO $$ BEGIN IF EXISTS (
    SELECT 1
    FROM information_schema.table_constraints
    WHERE table_name = 'users'
        AND constraint_type = 'CHECK'
        AND constraint_name = 'users_role_check'
) THEN
ALTER TABLE users DROP CONSTRAINT users_role_check;
END IF;
ALTER TABLE users
ADD CONSTRAINT users_role_check CHECK (
        role IN (
            'manager',
            'team_leader',
            'runner',
            'qc_inspector',
            'payroll_admin',
            'admin',
            'hr_admin',
            'logistics'
        )
    );
RAISE NOTICE 'Role constraint updated: 8 roles';
END $$;
COMMENT ON COLUMN users.role IS 'User role: manager, team_leader, runner, qc_inspector, payroll_admin, admin, hr_admin, logistics';