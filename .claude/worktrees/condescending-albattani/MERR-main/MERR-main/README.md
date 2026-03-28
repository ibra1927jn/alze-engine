# 🌿 HarvestPro NZ — Industrial Orchard Management Platform

![Version](https://img.shields.io/badge/version-9.0.0-green)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Tests](https://img.shields.io/badge/tests-291%20pass-brightgreen)
![TypeScript](https://img.shields.io/badge/TypeScript-5.3-blue)
![React](https://img.shields.io/badge/React-19-61DAFB)
![Lint](https://img.shields.io/badge/lint-0%20errors-brightgreen)
![LOC](https://img.shields.io/badge/LOC-~35k-informational)
![Security](https://img.shields.io/badge/adversarial%20audit-24%20fixes-critical)
![a11y](https://img.shields.io/badge/a11y-WCAG%202.1-blue)

> Real-time harvest tracking, wage compliance, and offline-first operations for New Zealand orchards.

---

## 🚀 What It Does

HarvestPro NZ bridges the gap between field and office with four core pillars:

| Pillar | Description |
| ------ | ----------- |
| **Real-Time Ledger** | Immutable record of every bin and bucket via mobile scanning — no paper, no human error |
| **Wage Shield** | Built-in payroll audit and minimum wage compliance to prevent legal disputes |
| **Offline-First** | Dexie-based sync engine with DLQ, conflict resolution, JWT silent refresh, and delta sync — crews work 100% disconnected |
| **Central Command** | CSV imports, timesheet corrections, multi-platform payroll exports (Xero, PaySauce) |
| **HR & Contracts** | Employee management, contract lifecycle tracking, compliance alerts |
| **Fleet & Logistics** | Vehicle tracking, transport request dispatch, zone-based bin inventory |

---

## 👥 Role-Based System (8 Roles)

The platform uses a hierarchical role system. Each role sees a dedicated dashboard:

```text
┌───────────────────────────────────────────────────────┐
│                    MANAGER                            │
│  • Strategic dashboard (velocity, cost, earnings)     │
│  • Productivity heatmaps                              │
│  • Broadcast messaging                                │
│  • CSV bulk import / Payroll export                    │
│  • Timesheet correction (audit trail)                 │
│  • 2FA enforced                                       │
├───────────────────────────────────────────────────────┤
│               TEAM LEADER                             │
│  • Attendance & check-in/out                          │
│  • Row assignments                                    │
│  • Crew management                                    │
│  • Transport request submission                       │
├───────────────────────────────────────────────────────┤
│              BUCKET RUNNER                            │
│  • QR / sticker code scanning                         │
│  • Bin delivery tracking                              │
│  • Warehouse management (works fully offline)         │
├───────────────────────────────────────────────────────┤
│              QC INSPECTOR                             │
│  • Quality grading (A/B/C/Reject)                     │
│  • Grade distribution analytics                       │
│  • Inspection history                                 │
├───────────────────────────────────────────────────────┤
│              HR ADMIN                                 │
│  • Employee directory with search                     │
│  • Contract management (permanent/seasonal/casual)    │
│  • Payroll overview with Wage Shield indicators       │
│  • Compliance alerts (expiring contracts, visa)       │
├───────────────────────────────────────────────────────┤
│           LOGISTICS COORDINATOR                       │
│  • Fleet management (tractor/vehicle tracking)        │
│  • Zone map with real-time asset positions             │
│  • Transport request dispatch & assignment            │
│  • Bin inventory (fill status, transit tracking)      │
├───────────────────────────────────────────────────────┤
│             PAYROLL ADMIN                             │
│  • Timesheet approval workflow                        │
│  • Payroll calculations & exports                     │
│  • Wage Shield compliance monitoring                  │
├───────────────────────────────────────────────────────┤
│                 ADMIN                                 │
│  • Full system administration                         │
│  • Dead letter queue management                       │
│  • Security dashboard                                 │
└───────────────────────────────────────────────────────┘
```

---

## 🛠️ Tech Stack

| Layer | Technology |
| ----- | ---------- |
| **Frontend** | React 19 + TypeScript 5.3 + Vite 7 |
| **Styling** | Tailwind CSS 3.4 + CSS Custom Properties (dynamic theming) |
| **State** | Zustand 5 (global) + React Query 5 (server) + React Context (auth, messaging) |
| **Validation** | Zod 4 (runtime schema validation) |
| **Database** | Supabase (PostgreSQL) with RLS — 26 tables, 40+ policies, 20+ functions |
| **Offline Storage** | Dexie.js (IndexedDB) — sync queue, dead-letter queue, conflict store, user cache |
| **Sync Engine** | Delta sync (`updated_at`-based) with zombie purge, 2-min jitter, Dexie DLQ, optimistic locking |
| **Auth** | Supabase Auth + MFA (TOTP) for managers |
| **PWA** | Service Workers via vite-plugin-pwa (43 precached entries) |
| **Virtual Scrolling** | react-virtuoso for large lists |
| **CSV Parsing** | PapaParse (bulk import with flexible column aliases) |
| **Testing** | Vitest + Testing Library (291 tests across 21 suites) |
| **i18n** | Custom i18n service with EN/ES/MI translations |

---

## 📦 Quick Start

### 1. Clone & Install

```bash
git clone https://github.com/ibra1927jn/harvestpro-nz.git
cd harvestpro-nz
npm install
```

### 2. Environment Variables

Create `.env.local` in the project root:

```env
# Supabase (required)
VITE_SUPABASE_URL=https://your-project.supabase.co
VITE_SUPABASE_ANON_KEY=your-anon-key

# Sentry (optional)
VITE_SENTRY_DSN=https://your-sentry-dsn

# PostHog (optional)
VITE_POSTHOG_KEY=your-posthog-key
VITE_POSTHOG_HOST=https://app.posthog.com
```

### 3. Database Setup

For a **fresh database**, use the consolidated schema:

```bash
# Single source of truth (26 tables, all RLS, all functions)
supabase/schema_v3_consolidated.sql

# Seed data
supabase/seeds/seed_blocks_and_rows.sql         # Orchard blocks A/B/C + rows
scripts/seed_demo_hr_logistics.sql              # Demo accounts (HR, Logistics roles)
```

> **Note**: If your database already has tables from earlier migrations, do NOT run V3. It is a reference document for fresh setups only.

### 4. Start Dev Server

```bash
npm run dev
# → http://localhost:5173
```

### 5. Test Accounts

| Role | Email | Password |
| ---- | ----- | -------- |
| Manager | <manager@harvestpro.nz> | 111111 |
| Team Leader | <lead@harvestpro.nz> | 111111 |
| Bucket Runner | <runner@harvestpro.nz> | 111111 |
| QC Inspector | <qc@harvestpro.nz> | 111111 |
| Payroll Admin | <payroll@harvestpro.nz> | 111111 |
| Admin | <admin@harvestpro.nz> | 111111 |
| HR Admin | <hr@harvestpro.nz> | 111111 |
| Logistics | <logistics@harvestpro.nz> | 111111 |

---

## 📁 Project Structure

```text
src/
├── components/              # ~145 TSX components
│   ├── common/              # Shared components (SyncBridge, ErrorBoundary, VirtualList, etc.)
│   ├── modals/              # 25 modals (AddPicker, ImportCSV, Export, Scanner, etc.)
│   ├── views/
│   │   ├── manager/         # 16 components
│   │   │   ├── DashboardView   → KPIs, velocity, cost, earnings
│   │   │   ├── TeamsView       → Crew management + CSV import
│   │   │   ├── TimesheetEditor → Admin correction with audit trail
│   │   │   ├── HeatMapView     → Row productivity visualization
│   │   │   ├── SettingsView    → Harvest config + compliance toggles
│   │   │   ├── WageShieldPanel → Compliance alerts
│   │   │   └── DayClosureButton → End-of-day lockdown
│   │   ├── team-leader/     # 11 components
│   │   ├── runner/          # 4 components
│   │   ├── qc/              # 4 components (Phase 2)
│   │   │   ├── InspectTab      → Picker search + grade entry (Turbo Mode)
│   │   │   ├── HistoryTab      → Recent inspections list
│   │   │   ├── StatsTab        → Grade distribution analytics
│   │   │   └── DistributionBar → Shared visualization
│   │   ├── hhrr/            # 5 components (Phase 2)
│   │   │   ├── EmployeesTab    → Employee directory + search
│   │   │   ├── ContractsTab    → Contract lifecycle management
│   │   │   ├── PayrollTab      → Payroll overview + wage shield
│   │   │   ├── DocumentsTab    → Document management
│   │   │   └── CalendarTab     → Calendar view
│   │   └── logistics/       # 5 components (Phase 2)
│   │       ├── FleetTab        → Vehicle tracking + zone map
│   │       ├── BinsTab         → Bin inventory + fill status
│   │       ├── RequestsTab     → Transport request cards
│   │       ├── RoutesTab       → Route planning
│   │       └── HistoryTab      → Transport log
│   ├── AuditLogViewer.tsx   # Immutable audit trail viewer
│   ├── SecurityDashboard.tsx # Admin security overview
│   └── MFASetup.tsx         # TOTP 2FA enrollment
├── context/                 # AuthContext, MessagingContext
├── hooks/                   # 20+ custom hooks (useToast, useInspectionHistory, etc.)
├── pages/                   # 9 pages
│   ├── Manager.tsx          → Orchard manager dashboard (7 tabs)
│   ├── TeamLeader.tsx       → Team leader dashboard
│   ├── Runner.tsx           → Bucket runner dashboard
│   ├── QualityControl.tsx   → QC inspector (decomposed → 3 tabs)
│   ├── HHRR.tsx             → HR department (5 tabs)
│   ├── LogisticsDept.tsx    → Logistics department (5 tabs)
│   ├── Payroll.tsx          → Payroll admin dashboard + wage calculator
│   ├── Admin.tsx            → System admin dashboard
│   └── Login.tsx            → Authentication (email/password + MFA)
├── services/                # ~55 service files + test files
│   ├── hhrr.service          → Employee/contract queries (Supabase)
│   ├── logistics-dept.service → Fleet/transport queries (Supabase)
│   ├── payroll.service       → Payroll calculations + timesheets
│   ├── qc.service            → Quality inspections
│   ├── sync.service          → Offline queue (6 types: SCAN, MSG, ATTENDANCE, CONTRACT, TRANSPORT, TIMESHEET)
│   ├── offline.service       → Dexie IndexedDB queue
│   ├── bucket-ledger.service → Immutable scan ledger
│   ├── attendance.service    → Check-in/out + corrections
│   ├── compliance.service    → Wage law alerts + NZ Employment Standards
│   ├── export.service        → CSV/Xero/PaySauce/PDF
│   ├── picker.service        → CRUD + bulk import + soft delete
│   ├── audit.service         → Immutable audit logging
│   ├── authHardening.service → Rate limiting, brute-force protection
│   ├── i18n.service          → EN/ES/MI translations
│   └── ...
├── stores/                  # 8 files — Zustand (useHarvestStore) + React Query + tests
├── types/                   # TypeScript interfaces + database.types.ts
└── utils/
    ├── nzst.ts               → NZST timezone utilities
    ├── csvParser.ts           → CSV parsing with column aliases
    └── logger.ts              → Structured logging
```

---

## 🧪 Scripts

```bash
npm run dev            # Start development server (→ localhost:5173)
npm run build          # TypeScript check + Vite production build
npm run lint           # ESLint check (0 errors)
npm run lint:fix       # ESLint auto-fix
npm run format         # Prettier formatting
npm test               # Run unit tests (Vitest) — 291 tests
npm run test:watch     # Tests in watch mode
npm run test:coverage  # Tests with coverage report
```

---

## ✨ Features by Phase

### Phase 1: Central Command ✅

- **CSV Bulk Import** — Drag & drop upload, flexible column aliases (EN/ES), duplicate detection
- **Timesheet Correction** — Inline edit with mandatory reason, full audit trail
- **Payroll Export** — 4 formats: Generic CSV, Xero, PaySauce, PDF
- **Productivity Heatmap** — Row-level visualization with intensity scaling
- **Broadcast Messaging** — Manager → all crew, real-time delivery
- **Day Closure** — End-of-day lockdown with archive

### Phase 3: Analytics & Communications Overhaul ✅

- **Weekly PDF Report — Print-First Redesign**
  - Compact KPI strip (white, no pastel cards) with 8 metrics
  - Period date in header ("Week 9: 23 Feb – 1 Mar, 2026")
  - Executive insights inline row (Top Performer, Wage Bleeding, Workforce)
  - Compact sparklines (Harvest Velocity, Workforce Size)
  - Team Rankings as clean audit-style table
  - Picker performance table with Days, Avg/Day, $/Hr columns + tier border accents
  - **Daily Summary** table (day-by-day aggregates: Pickers, Bins, Hours, Avg B/Hr, Est. Cost, Cost/Bin)
  - **Performance Distribution** histogram (text-based bars by Bins/Hr brackets)
  - **Cost Analysis** (Top 5 Most Efficient + Top 5 Most Costly pickers by $/bin)
  - **Per-Team Breakdown** with page-break-before per team for individual distribution

- **Messaging System Overhaul**
  - Premium indigo/blue gradient design matching app theme
  - Real-time Supabase subscription fix (messages appear live)
  - Sender name resolution via userNameMap (fixes "Worker xxxx" bug)
  - Search bar for conversations and alerts
  - Quick replies for field context (👍 ✅ 🚜 ⚠️ ☔ 🔄)
  - Message date separators (Today, Yesterday, Mon 24 Feb)
  - Delivery indicators (✓✓) on sent messages
  - Online status dots on avatars
  - Role badges in new chat modal
  - Person search when creating conversations

### Phase 2: Department Services ✅

- **HR Department** (HHRR.tsx)
  - Employee directory with role badges, status, visa info
  - Contract lifecycle: draft → active → expiring → expired → terminated
  - Compliance alerts: expiring contracts, visa monitoring
  - Payroll overview with Wage Shield indicators

- **Logistics Department** (LogisticsDept.tsx)
  - Fleet management: tractor/vehicle status (active/idle/maintenance/offline)
  - Zone map: real-time vehicle positions across orchard zones
  - Transport requests: team leaders request pickups, logistics dispatch vehicles
  - Bin inventory: fill percentage, transit tracking

- **Quality Control** (QualityControl.tsx)
  - Decomposed architecture: InspectTab, HistoryTab, StatsTab
  - Grade entry: A (Export) / B (Domestic) / C (Process) / Reject
  - Distribution analytics with visual bar

- **Payroll Admin** (Payroll.tsx)
  - Timesheet approval workflow
  - Attendance-based calculations

- **Offline-First Sync** (sync.service.ts)
  - 6 queue types: SCAN, MESSAGE, ATTENDANCE, CONTRACT, TRANSPORT, TIMESHEET
  - Conflict resolution with `keep_local` / `keep_remote` (re-queues properly)
  - Auto-retry with 50 attempt cap → Dead Letter Queue
  - DLQ admin editor: edit payload JSON and retry
  - Atomic DLQ persistence (V28 — no data loss on DLQ insert failure)

---

## 🔒 Security

- **Row Level Security (RLS)**: 26 tables with 40+ policies — users only access their assigned orchard
- **Role-Based Access**: 8 granular roles with per-table policies
- **MFA**: Managers require TOTP-based two-factor authentication
- **Audit Logs**: Every data change generates an immutable audit trail
- **Auth Hardening**: Rate limiting, session management, brute-force protection
- **JWT Silent Refresh**: Proactive 50-min timer + visibility-based refresh with 3-min throttle (Sprint 12)
- **Session Lifecycle**: Sign-out wipes Dexie DB, blocks if unsynced data, forces page reload (U6+V26+V27)
- **Anti-Fraud Trigger**: Server-side enforcement blocks bucket inserts on closed days
- **Optimistic Locking**: `bump_version()` trigger on pickers, attendance, row_assignments
- **Conflict Resolution**: `keep_local` properly re-queues via table→type mapping (U7+V25)
- **Dead Letter Queue**: Atomic persistence — items only removed from sync_queue on DLQ success (V28)
- **Financial Guards**: Negative hour prevention (`Math.max(0, ...)`) in payroll + HHRR (U10+U11)
- **Soft Delete**: All critical tables use `deleted_at` — data is never physically destroyed
- **Error Logging**: All catch blocks log to structured logger (17 silent catches fixed in Sprint 8)

---

## ♿ Accessibility (WCAG 2.1 Level AA)

All form components audited and compliant:

- **Labels**: Every `<input>`, `<select>`, and `<textarea>` linked via `htmlFor`/`id` or `aria-label`
- **ARIA attributes**: Switches use `role="switch"` with proper `aria-checked` string values
- **Screen readers**: Dynamic selects include `aria-label` for assistive context
- **CSS architecture**: Dynamic styles use CSS Custom Properties pattern (no raw inline styles)
- **Keyboard navigation**: All interactive elements keyboard-accessible

Audited components: `NewContractModal`, `AddVehicleModal`, `SetupWizard`, `InlineSelect`, `InlineEdit`, `InspectTab`, `SettingsView`, `NewTransportRequestModal`, `Payroll`, `TeamView`, `ProfileView`, `AddRunnerModal`, `DayConfigModal`, `PickerDetailsModal`, `ProfileModal`, `RowAssignmentModal`, `SettingsFormComponents`

---

## 🗃️ Database Schema (26 Tables)

> **Source of truth**: `supabase/schema_v3_consolidated.sql`

| Category | Tables |
| --- | --- |
| **Hierarchy** | `harvest_seasons`, `orchard_blocks`, `block_rows` |
| **Core** | `orchards`, `users`, `pickers`, `day_setups`, `bucket_records`, `bins` |
| **Assignment** | `row_assignments` |
| **Quality** | `quality_inspections`, `qc_inspections` |
| **Messaging** | `conversations`, `chat_messages`, `messages` |
| **Attendance** | `daily_attendance`, `day_closures` |
| **Logistics** | `fleet_vehicles`, `transport_requests` |
| **HR** | `contracts` |
| **Security** | `login_attempts`, `account_locks`, `audit_logs`, `sync_conflicts`, `allowed_registrations` |
| **Settings** | `harvest_settings` |

**Views**: `pickers_performance_today`

**Key features**: Soft deletes (`deleted_at`), optimistic locking (`version` + `bump_version()` trigger), partial unique indexes, season-scoped queries, anti-fraud closed-day trigger

---

## 📊 Sprint History

| Sprint | Focus | Key Results |
| ------ | ----- | ----------- |
| **1** | Architecture & Base | Role routing, Supabase integration, component structure |
| **2** | Security Hardening | MFA, auth flows, destructor audit, sync bridge fixes |
| **3** | Clean-Sheet Protocol | 201→0 lint errors, type guards, PATTERNS.md |
| **4** | Warning Reduction | 115→0 warnings, catch block refactoring, profile sync |
| **5** | Central Command (Phase 1) | CSV bulk import, timesheet corrections, Xero/PaySauce export |
| **6** | Department Services (Phase 2) | HR/Logistics/Payroll wiring to Supabase, QC decomposition, 3 new DB tables, offline sync expansion |
| **7** | Quality Assurance & a11y | 40-point browser audit (all passed), WCAG 2.1 accessibility compliance across 10 components, Playwright E2E tests |
| **8** | Code Quality & Performance | Silent catch fixes (17), RLS remediation (3 tables), `fetchGlobalData` refactor (217→15 lines), DashboardView split (338→190 lines), React.memo on heavy lists, comments standardized to English |
| **9** | Visual Polish & UX | CSS inline styles refactored to CSS Custom Properties + utility classes, `window.alert()` → toast system, double Inter font load eliminated, `max-w-md` constraint removed across 9 files, virtual scrolling (VirtualList), OrchardMapView visual overhaul, animation system (slide-up, breathe, fade-in), demo mode re-enabled in production, 17 accessibility fixes (aria-label, aria-checked), schema alignment migration |
| **10** | Adversarial Hardening (6 rounds) | **24 fixes**: Dexie migration (sync queue, DLQ, conflict store), session sign-out hardening (wipe + reload), conflict resolution `keep_local` re-queue, DLQ edit & retry, atomic DLQ persistence, negative hours guards, realtime anti-squash, RLS recursion fix, bucket_records RLS, closed-day trigger, optimistic locking, NZST-safe calculations |
| **11** | Code Quality & Modernization | React Query integration, Zod validation layer, lint cleanup (0 errors, 0 warnings), atomic RPCs for roster/attendance/bins/fleet, optimistic lock trigger, setupWizard service, Result<T> type pattern |
| **12** | Database & Offline Hardening | Schema V3 consolidated (26 tables, 40+ RLS, 20+ functions, 1 VIEW), JWT silent refresh (50-min timer + visibility throttle), delta sync (`updated_at`-based with zombie purge + 2-min jitter), database hierarchy (`harvest_seasons` → `orchard_blocks` → `block_rows`) |
| **13** | Analytics & Communications | PDF print-first redesign (compact KPI, audit-style tables, sparklines), 5 new report sections (Daily Summary, Performance Distribution, Cost Analysis, Per-Team Breakdown), messaging overhaul (premium design, real-time fix, quick replies, search, date separators, delivery indicators) |

---

## 🗺️ Roadmap — Next Steps

### Pre-Pilot (Deployment)

| # | Feature | Priority | Status |
| --- | ------- | -------- | ------ |
| 1 | **Apply migrations in Supabase** — Execute all SQL files in production DB | 🔴 Critical | Pending |
| 2 | **Auth flow verification** — Full signup → email verify → role assign → login | 🔴 Critical | Pending |
| 3 | **Monitoring setup** — Configure Sentry DSN + PostHog for production visibility | High | Pending |

### Feature Enhancements

| # | Feature | Priority | Status |
| --- | ------- | -------- | ------ |
| 1 | **Push notifications** — Web Push for urgent transport requests | Medium | Planned |
| 2 | **Contract action buttons** — Renew, terminate from ContractsTab UI | Medium | Planned |
| 3 | **Transport dispatch UI** — Accept/assign/complete in RequestsTab | Medium | Planned |
| 4 | **Weekly/monthly reporting** — Automated email reports for managers | Medium | Planned |
| 5 | **Cost analytics** — Labour cost per bin, per zone, per team | Medium | Planned |
| 6 | **Rate limiting** — Client-side throttle for scan operations | Low | Planned |
| 7 | **Performance monitoring** — Web Vitals + Lighthouse CI | Low | Planned |

### Already Completed ✅

| Feature | Sprint |
| ------- | ------ |
| PWA (Service Worker + manifest.json + offline caching via `vite-plugin-pwa`) | 7 |
| Realtime dashboard (Supabase channels in `useHarvestStore` + `MessagingContext`) | 6 |
| Seed data (`seed_season_simulation.sql` + `seed_demo_hr_logistics.sql`) | 6 |
| Error boundaries — React boundaries per route + ComponentErrorBoundary | 7–9 |
| Accessibility audit — WCAG 2.1 AA compliance across 17+ components | 7–9 |
| Silent catch fixes (17) + structured logging | 8 |
| RLS remediation (22 tables audited, 3 gaps fixed) | 8 |
| `fetchGlobalData` refactor (217→15 lines) + DashboardView split (338→190 lines) | 8 |
| React.memo on heavy list components | 8 |
| CSS Custom Properties system (20+ utility classes for dynamic styles) | 9 |
| Toast notification system (replaces all `window.alert()`) | 9 |
| VirtualList component (`@tanstack/react-virtual`) | 9 |
| OrchardMapView visual overhaul (animated row cards, gradient fills) | 9 |
| Animation system (slide-up, breathe, fade-in, micro-interactions) | 9 |
| Demo mode available in production | 9 |
| Schema alignment migration (`20260214_schema_alignment.sql`) | 9 |
| Unit test coverage — 279 tests across 21 suites | 3–10 |
| Dexie-based sync with DLQ + conflict store (replaces localStorage queues) | 10 |
| Session sign-out hardening (Dexie wipe + sync guard + hard reload) | 10 |
| Conflict resolution `keep_local` → re-queue with table→type map | 10 |
| DLQ admin editor (edit payload JSON + retry) | 10 |
| Atomic DLQ persistence (V28) | 10 |
| Negative hours financial guards (Math.max in payroll + HHRR) | 10 |
| Realtime anti-squash (QC/timesheet → capped list) | 10 |
| RLS recursion fix + bucket_records RLS + closed-day trigger | 10 |
| React Query integration (`@tanstack/react-query` + `queryClient.ts`) | 11 |
| Zod runtime schema validation | 11 |
| Lint cleanup — 0 errors, 0 warnings | 11 |
| Atomic RPCs for roster, attendance, bins, fleet operations | 11 |
| Optimistic lock trigger for attendance records | 11 |
| Result<T> type pattern for type-safe error handling | 11 |
| Schema V3 Consolidated (26 tables, 40+ RLS, 20+ functions) — single source of truth | 12 |
| Database hierarchy: `harvest_seasons` → `orchard_blocks` → `block_rows` | 12 |
| Soft deletes + optimistic locking on all critical tables | 12 |
| JWT silent refresh (50-min timer + visibility-based + TOKEN_REFRESHED handler) | 12 |
| Delta sync (`updated_at`-based, zombie purge, 2-min jitter, O(1) Map merge) | 12 |
| Anti-fraud closed-day trigger (`enforce_closed_day_bucket_records`) | 12 |
| `pickers_performance_today` VIEW for attendance service | 12 |
| Weekly PDF print-first redesign (compact KPIs, audit-style tables, sparklines) | 13 |
| 5 new report sections: Daily Summary, Perf. Distribution, Cost Analysis, Team Breakdown | 13 |
| Messaging overhaul: premium design, real-time subscription fix, quick replies | 13 |
| Message search, date separators, delivery indicators, online status dots | 13 |
| Role badges + person search in new chat modal | 13 |

---

## 📚 Additional Docs

- [`ARCHITECTURE.md`](./ARCHITECTURE.md) — System architecture, data flow, sync pipeline
- [`DEPLOYMENT.md`](./DEPLOYMENT.md) — Production deployment guide
- [`PATTERNS.md`](./PATTERNS.md) — React & TypeScript patterns reference
- [`MANUAL_OPERACIONES.md`](./MANUAL_OPERACIONES.md) — Operations manual (Spanish)
- [`SETUP_SECRETS.md`](./SETUP_SECRETS.md) — Environment variable configuration
- [`docs/DEMO_SCRIPT.md`](./docs/DEMO_SCRIPT.md) — Demo walkthrough script
- [`docs/FUNCTIONAL_AUDIT.md`](./docs/FUNCTIONAL_AUDIT.md) — Functional audit report

---

## 📝 License

Proprietary — Harvest NZ Merr. All rights reserved.

---

_Last updated: 2026-02-28 | Sprint 13 — Analytics & Communications (PDF redesign, comprehensive reports, messaging overhaul)_
