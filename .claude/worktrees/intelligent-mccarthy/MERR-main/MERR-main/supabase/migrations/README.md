# HarvestPro NZ — Database Deployment Guide

## Quick Start (Fresh Environment)

Run scripts in this exact order against your Supabase SQL Editor:

### Step 1: Base Schema

```sql
-- Run: supabase/schema_v1_consolidated.sql
-- Creates: orchards, users, pickers, day_setups, bucket_records,
--          bins, quality_inspections, conversations, chat_messages
-- Includes: RLS policies, indexes, realtime setup
```

### Step 2: Migrations (in order)

```
supabase/migrations/20260210_day_closures.sql          -- Day closure & immutability
supabase/migrations/20260211_add_archived_at.sql       -- Soft delete for pickers
supabase/migrations/20260211_audit_logging.sql         -- Full audit trail system
supabase/migrations/20260211_auth_hardening.sql        -- Rate limiting & account locks
supabase/migrations/20260211_complete_rls.sql          -- Comprehensive RLS policies
supabase/migrations/20260211_day_closures_role_restriction.sql  -- Role-based closure
supabase/migrations/20260211_idempotent_buckets.sql    -- Bucket dedup logic
supabase/migrations/20260211_rls_block_archived_pickers.sql     -- Block archived picker ops
supabase/migrations/20260211_rls_offline_closed_days.sql        -- Offline day protection
supabase/migrations/20260211_row_assignments_columns.sql        -- Row assignment columns
supabase/migrations/20260211_timestamptz_audit.sql     -- Timestamp audit columns
```

### Step 3: Edge Functions

```
supabase/functions/calculate-payroll/  -- Payroll Edge Function (deploy via CLI)
```

## Directory Structure

```
supabase/
├── schema_v1_consolidated.sql    ← Base schema (run FIRST)
├── full_database_reset_v1.sql    ← DANGER: drops all tables then recreates
├── migrations/                   ← Apply in filename order after base schema
│   ├── 20260210_day_closures.sql
│   ├── 20260211_*.sql (10 files)
│   └── README.md (this file)
├── archive/                      ← Historical hotfixes, DO NOT RUN
│   ├── fix_*.sql
│   ├── enable_global_roster.sql
│   ├── reset_*.sql
│   └── schema_v2_clean.sql
└── functions/
    └── calculate-payroll/
```

## Important Notes

- **All migrations are idempotent** (`CREATE TABLE IF NOT EXISTS`, `DROP POLICY IF EXISTS`)
- **RLS is enabled on ALL tables** — anonymous access is blocked
- **`archive/` scripts are reference only** — they were applied during development and are now superseded by the consolidated schema + migrations
- **TypeScript types** in `src/types/database.types.ts` must stay in sync with this schema
