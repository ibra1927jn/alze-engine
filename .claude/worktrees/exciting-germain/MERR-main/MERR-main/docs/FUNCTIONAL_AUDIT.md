# HarvestPro NZ — Auditoría Funcional Completa

## Estado real de cada pantalla, botón y función

> Fecha del análisis: 12 Feb 2026 · Última actualización: 17 Feb 2026 (Round 3 — 16 logic fixes)

---

## RESUMEN EJECUTIVO

| Categoría | Funcional | Cosmético/Placeholder | Necesita trabajo |
|-----------|-----------|----------------------|-----------------|
| Manager (7 tabs) | 6 | 0 | 1 (Settings mal asignado) |
| Team Leader (6 tabs) | 5 | 0 | 1 (Chat = wrapper) |
| Runner (4 tabs) | 3 | 0 | 1 (Warehouse parcial) |
| QC Inspector (3 tabs) | 0 | **3** | **3 (Todo placeholder)** |
| Modals | 5 | 1 | 1 (Export incompleto) |
| **Total** | **19** | **4** | **7** |

---

## 1. MANAGER (`/manager`)

### Tab: Dashboard ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Fuente de datos |
|----------|--------|----------------|
| KPI Cards (Buckets, Tons, Speed, Earnings) | ✅ Real | `useHarvestStore.stats` + `payroll.finalTotal` |
| Progress Bar (% del target) | ✅ Real | `stats.tons / settings.target_tons` |
| ETA Calculation | ✅ Real | `analyticsService.calculateETA()` con velocity real |
| VelocityChart (bar chart hourly) | ✅ Real | `analyticsService.groupByHour(bucketRecords)` → Pure CSS/SVG bars |
| WageShieldPanel (compliance alerts) | ✅ Real | `compliance.service` + `analyticsService.calculateWageStatus()` |
| Live Floor (picker grid) | ✅ Real | `crew` map, click → `onUserSelect` |
| Team Leaders Panel | ✅ Real | `teamLeaders` con bucket aggregation |
| SimulationBanner | ✅ Real | Warning banner cuando hay datos simulados |
| DayClosureButton | ✅ Real | Ejecuta `payrollService` → inserta en Supabase `daily_closures` → genera CSV report |
| TrustBadges | ✅ Real | RLS/Offline/NZ Compliant static indicators |
| Export button (download icon) | ✅ Real | `analyticsService.generateDailyReport()` → `downloadCSV()` |

### Tab: Teams ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| TeamsToolbar | ✅ Real | Search filtra crew en real-time |
| "New Member" button | ✅ Real | Abre `AddPickerModal` → `pickerService.addPicker()` → Supabase insert |
| "Link Leader" button | ✅ Real | FUNCIONA — asigna team leader a orchard |
| "Import CSV" button | ⚠️ **Parcial** | Botón visible pero NO hay lógica de import CSV implementada |
| TeamLeaderCard expand | ✅ Real | Muestra crew asignada con buckets reales |
| RunnersSection | ✅ Real | Filtra runners del store |
| Picker click → detail | ✅ Real | `setSelectedUser` abre modal de detalle |
| Delete picker | ✅ Real | Soft-delete (archived_at) via `pickerService` |

### Tab: Timesheet ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Date picker | ✅ Real | Filtra attendance por fecha (`todayNZST`) |
| Attendance table | ✅ Real | `attendanceService.getAttendanceRecords()` → Supabase query |
| Edit check-in/out times | ✅ Real | Inline editing con `saveCorrection()` → audit trail |
| Correction reason | ✅ Real | Required text input, stored in DB |
| Abnormal hours flag | ✅ Real | `isAbnormal()` checks >14h (flagged for review, NOT capped — L14 fix) |
| Hours calculation | ✅ Real | `calculateHours()` diff entre check-in/out |

### Tab: Logistics ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Full Bins count | ✅ Real | `fullBins` from store (query bins WHERE status='full') |
| Empty Bins count | ✅ Real | `emptyBins` from store |
| Active Runners list | ✅ Real | Filtered crew con role='runner' |
| "Request Pickup" button | ✅ Real | Triggers `handleBroadcast()` → urgent broadcast via Supabase |
| Tractor Fleet status | ⚠️ **Estático** | Hardcoded text, no hay tabla de tractores en DB |

### Tab: Messaging ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| UnifiedMessagingView | ✅ Real | `simpleMessagingService` → Supabase `messages` table |
| Send message | ✅ Real | Insert into DB + real-time subscription |
| Direct chat | ✅ Real | `handleStartDirectChat()` creates/opens DM |
| Group chat creation | ✅ Real | `NewChatModalContent` → `onCreateGroup()` |
| BroadcastModal | ✅ Real | `sendBroadcast()` → Supabase `broadcasts` table |
| Read receipts | ✅ Real | `read_by` array tracked |
| Priority levels | ✅ Real | normal/high/urgent stored and displayed |

### Tab: Rows (Map) ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| RowListView | ✅ Real | Renders `totalRows` from orchard settings, calculates buckets per row |
| Block name / variety | ✅ Real | `orchard.name`, `settings.variety` from store |
| Row progress bars | ✅ Real | `getBucketsForRow()` aggregates from crew `current_row` |
| Row click → assignment | ✅ Real | Opens assignment modal |
| ETA per row | ✅ Real | `calculateETA()` per row |

### Tab: Settings ⚠️ **MAL ASIGNADO**

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Renders `AuditLogViewer` | ⚠️ Bug | `case 'settings': return <AuditLogViewer />` — NO es Settings! |
| AuditLogViewer functions | ✅ Real | Queries `audit_logs` table with filters, timestamp formatting, CSV export |
| **FALTA**: Settings page | ❌ No existe | No hay UI para: piece_rate, min_wage, target_tons, orchard_name change |

> **BUG**: La tab "Settings" en Manager.tsx (línea 166) renderiza `<AuditLogViewer />` en vez de un panel de configuración real. Los settings del orchard se configuran directamente en Supabase actualmente.

---

## 2. TEAM LEADER (`/team-leader`)

### Tab: Home ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Greeting "Kia Ora, [Nombre]" | ✅ Real | `currentUser.name` from store |
| KPI Cards (Buckets/Pay/Tons) | ✅ Real | `stats.totalBuckets`, `stats.payEstimate`, `stats.tons` |
| Safety Monitor | ✅ Real | `crew.find(p => p.status === 'suspended' || p.status === 'issue')` |
| Performance Progress Bar | ✅ Real | `(currentAvg / dailyGoalPerPicker) * 100` usando `min_buckets_per_hour` |
| Active Crew list (top 5) | ✅ Real | `crew.sort()` by total_buckets_today, shows row # and QC dots |
| "View All" link | ✅ Real | `onNavigate('team')` |
| Profile avatar click | ✅ Real | `onNavigate('profile')` |

### Tab: Roll Call (Attendance) ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Present/Absent stats | ✅ Real | Computed from `checkedInIds` Set |
| "Check In" button | ✅ Real | `attendanceService.checkIn()` → Supabase upsert |
| "Check Out" button | ✅ Real | `attendanceService.checkOut()` → Supabase update |
| Processing spinner | ✅ Real | Shows during async operation |
| Real-time status | ✅ Real | Crew status from store subscription |

### Tab: Team (TeamView) ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Picker list | ✅ Real | `crew` from store |
| "Add Picker" button | ✅ Real | Opens AddPickerModal → `pickerService.addPicker()` |
| Picker detail click | ✅ Real | `setSelectedUser()` |
| Delete picker | ✅ Real | Soft-delete with confirmation |
| Picker stats (buckets, row, quality) | ✅ Real | From store data |

### Tab: Tasks ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Row list with progress | ✅ Real | Calculates per-row buckets from crew data |
| HeatMapView integration | ✅ Real | `analyticsService.getRowDensity()` → Supabase query |
| Row assignment flow | ✅ Real | Modal para asignar pickers a rows |

### Tab: Chat ✅ FUNCIONAL (wrapper)

Wraps same `UnifiedMessagingView` as Manager. All messaging functions work.

### Tab: Profile ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| User info display | ✅ Real | Name, role, ID from store |
| Orchard info | ✅ Real | From settings |
| Piece Rate / Min Wage display | ✅ Real | Read-only from settings |
| "Sign Out" button | ✅ Real | `signOut()` → clears session → redirect to login |
| Offline Mode toggle | ⚠️ Display only | Toggle renders but sin efecto real |
| "End of Day Report" button | ⚠️ Display only | Botón visible pero sin handler implementado |

---

## 3. RUNNER (`/runner`)

### Tab: Logistics ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| "SCAN BUCKET" button | ✅ Real | Opens `ScannerModal` → camera/manual input → validates picker → opens `QualityRatingModal` |
| "SCAN BIN" button | ✅ Real | Opens `ScannerModal` with type='BIN' → `setSelectedBinId()` |
| Quality grading (A/B/C/Reject) | ✅ Real | `QualityRatingModal` → `offlineService.queueBucketScan()` |
| Active Bin display | ✅ Real | Shows `selectedBinId` from local state |
| Pending uploads counter | ✅ Real | `offlineService.getPendingCount()` polled every 5s |
| SyncStatusMonitor | ✅ Real | Shows offline queue status |
| Toast notifications | ✅ Real | Success/error/warning after scan operations |
| Broadcast function | ✅ Real | `sendBroadcast()` via messaging context |
| Haptic feedback | ✅ Real | `feedbackService` vibration on scan success |

### Tab: Runners (RunnersView) ✅ FUNCIONAL COMPLETO

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Active pickers list | ✅ Real | `crew` from store, shows name, row, bucket count |
| Back button | ✅ Real | `onBack()` switches tab |
| Status indicator (green pulse) | ✅ Real | Live indicator |
| Picker count badge | ✅ Real | `activePickers.length` |

### Tab: Warehouse ⚠️ PARCIALMENTE FUNCIONAL

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Full Bins count | ✅ Real | `inventory.full_bins` |
| Empty Bins count | ✅ Real | `inventory.empty_bins` |
| In Progress count | ✅ Real | `inventory.in_progress` |
| "Ready for Pickup" status | ✅ Real | Conditional on `fullBins > 0` |
| "REQUEST TRANSPORT" button | ⚠️ **Parcial** | `onTransportRequest` prop exists but handler is **undefined** — clicking does nothing |
| "Next Resupply Truck" info | ❌ **Hardcoded** | Static text, no real truck tracking |
| Avatar/notification bell | ❌ **Cosmético** | Static UI, no linked data |

### Tab: Messaging ✅ FUNCIONAL

Same `UnifiedMessagingView` wrapper. Full messaging works.

---

## 4. QC INSPECTOR (`/qc`) ❌ **TODO PLACEHOLDER**

| Elemento | Estado | Detalle |
|----------|--------|---------|
| Header + branding | ✅ Visual | Static emerald header, looks good but no data |
| Tab navigation (3 tabs) | ✅ Visual | Tabs switch views, but... |
| **Grade buttons (A/B/C/Reject)** | ❌ **COSMÉTICO** | `<button>` elements con ZERO `onClick` handler — **no hacen nada** |
| "Coming Soon" banner | ✅ Honest | Dice explícitamente "Full inspection workflow... under development" |
| History tab | ❌ **Vacío** | Static "No inspections recorded" — sin query a DB |
| Statistics tab | ❌ **Vacío** | Static "Will show trends once inspections are logged" |
| Picker selection | ❌ **No existe** | "Scan or select a picker" dice pero no hay selector ni scanner |

> **VEREDICTO**: La página QC completa es maqueta. Ni un solo botón tiene funcionalidad. No hay service layer (no existe qc.service.ts), no hay tabla en DB para inspections standalone (solo `quality_grade` en `bucket_events`).

---

## 5. HEATMAP / MAPA DE CALOR — ANÁLISIS DETALLADO

### ¿Funciona realmente?

**SÍ**, pero con limitaciones:

| Componente | Estado | Detalle |
|------------|--------|---------|
| `HeatMapView.tsx` | ✅ Real | Renders DOM-based rows with dynamic colors |
| `analyticsService.getRowDensity()` | ✅ Real | Query a Supabase `bucket_events` table con filtro por orchard + fecha |
| Color coding (verde/amarillo/rojo) | ✅ Real | `getRowColor()` basado en `target_completion` |
| Opacity scaling | ✅ Real | `getRowOpacity()` basado en `density_score` |
| Date range toggle (hoy/7 días) | ✅ Real | Requery con nuevas fechas |
| Stats summary cards | ✅ Real | Aggregated from query results |
| Progress bars per row | ✅ Real | CSS dynamic width |
| Empty state handling | ✅ Real | Graceful "No hay datos" message |
| `HeatMapView.module.css` | ✅ Real | Dedicated CSS module para estilos |

### Limitaciones del HeatMap

1. **NO es un mapa geográfico real** — Es una lista de filas con colores, no un mapa con coordenadas GPS
2. **Depende de `row_number` en `bucket_events`** — Si los runners no registran el row_number al escanear, no hay datos
3. **El target es fijo** (100 buckets por row default) — No se adapta por variedad de cereza o tamaño de fila
4. **No hay view satelital/aerial** — No usa Mapbox, Google Maps, ni leaflet
5. **Latencia con Supabase** — Si hay muchos eventos, la query puede ser lenta (no hay índices optimizados específicos)

### ¿Qué faltaría para un HeatMap real?

- Integrar GPS del dispositivo del runner al escanear
- Mapa leaflet/mapbox con polígonos de filas del orchard
- Overlay de calor sobre posiciones reales
- Real-time websocket updates (actualmente es poll-on-load)

---

## 6. BOTONES PURAMENTE COSMÉTICOS (NO hacen nada)

| Ubicación | Botón | Estado |
|-----------|-------|--------|
| QC → Inspect tab | Grade A button | ❌ Sin onClick |
| QC → Inspect tab | Grade B button | ❌ Sin onClick |
| QC → Inspect tab | Grade C button | ❌ Sin onClick |
| QC → Inspect tab | Reject button | ❌ Sin onClick |
| Teams → Toolbar | "Import CSV" | ⚠️ Visible, sin import logic |
| Profile (TL) | "End of Day Report" | ⚠️ Visible, sin handler |
| Profile (TL) | Offline Mode toggle | ⚠️ Renders, sin efecto real |
| Warehouse | Notification bell | ❌ Static, sin data |
| Warehouse | Avatar/profile | ❌ Static, hardcoded image |

---

## 7. ROLES EXISTENTES vs NECESARIOS

### Roles actuales (en `types.ts`)

```typescript
enum Role {
  MANAGER = 'manager',
  TEAM_LEADER = 'team_leader',
  RUNNER = 'runner',
  QC_INSPECTOR = 'qc_inspector',
  PAYROLL_ADMIN = 'payroll_admin'  // ← Existe pero SIN PANTALLA
}
```

### ❌ PAYROLL_ADMIN — Existe en enum pero NO tiene página propia

- En `Login.tsx`: `PAYROLL_ADMIN: '/manager'` → redirige al Manager dashboard
- No hay vista específica de payroll admin
- Usa el mismo dashboard que Manager

### 🆕 ROLES/DEPARTAMENTOS QUE FALTAN

#### 1. **ADMIN / HR (Recursos Humanos)**

**¿Por qué?**: Actualmente el Manager hace TODO: gestión de personal + operaciones. Necesita separación.

Funciones necesarias:

- **Gestión de usuarios**: Crear/editar/desactivar usuarios de TODOS los roles
- **Permisos y roles**: Asignar/cambiar roles (promover picker a team leader, etc.)
- **Orchards**: Crear/editar orchards, asignar usuarios a orchards
- **Onboarding**: Workflow de alta de nuevos trabajadores (documentos, contratos)
- **Multi-orchard view**: Ver todos los orchards desde un panel central
- **Dashboard de HR**: Headcount total, turnover, worker performance across seasons
- **Configuración global**: Piece rates, min wage, season dates, varieties
- **Audit trail viewer**: Ya existe `AuditLogViewer` — debería vivir aquí
- **Data export/reporting**: Reports across orchards for corporate/government compliance

#### 2. **PAYROLL ADMIN** (ya existe el enum, falta la pantalla)

**¿Por qué?**: La persona que aprueba pagos no debería tener acceso a operaciones de campo.

Funciones necesarias:

- **Payroll dashboard**: Resumen de costos por día/semana/temporada
- **Approval workflow**: Aprobar day closures antes de que se paguen
- **Tax calculations**: PAYE, holiday pay, ACC levies (NZ-specific)
- **Time corrections review**: Ver/aprobar las correcciones de timesheet
- **Payment export**: Generar archivos para bank payment (NZ format)
- **Compliance dashboard**: Workers below minimum wage, overtime violations
- **Invoice generation**: Para orchards que usan contractors

#### 3. **SUPERVISOR DE CAMPO (Field Supervisor)**

**¿Por qué?**: Posición entre Manager y Team Leader. Supervisores que cubren múltiples equipos.

Funciones necesarias:

- **Multi-team view**: Ver 3-5 teams a la vez
- **Re-asignación rápida**: Mover pickers entre teams sobre la marcha
- **Quality oversight**: Dashboard de calidad por zona
- **Incident reporting**: Reportar accidentes, conflictos, problemas de equipamiento

#### 4. **WAREHOUSE OPERATOR (Operador de Almacén)**

**¿Por qué?**: La vista Warehouse del Runner es muy básica. El personal de almacén necesita herramientas específicas.

Funciones necesarias:

- **Bin management**: Tracking detallado de cada bin (peso, calidad, destino)
- **Coolstore integration**: Registro de bins que entran a refrigeración
- **Dispatch**: Coordinar camiones, crear órdenes de despacho
- **Weight bridge**: Registro de peso de bins (reconciliación con bucket counts)
- **Traceability**: Lote/batch tracking para exportación (MPI compliance)

---

## 8. SISTEMA DE PERMISOS — ANÁLISIS

### Estado actual

**NO hay sistema de permisos granular.** El control de acceso es binario:

1. `Role` enum determina qué **página** ves (Manager → `/manager`, Runner → `/runner`, etc.)
2. Supabase RLS (Row Level Security) controla acceso a **datos** por `orchard_id`
3. No hay permisos intermedios (ej: "Manager que sólo ve, no edita")

### ¿Qué se necesita?

```
┌──────────────────────────────────────────────────────────┐
│                    PERMISSION MATRIX                      │
├──────────────┬──────────┬──────────┬──────────┬──────────┤
│ Action       │ Admin/HR │ Manager  │ PayAdmin │ TL/Run   │
├──────────────┼──────────┼──────────┼──────────┼──────────┤
│ Create users │ ✅       │ ❌       │ ❌       │ ❌       │
│ Change roles │ ✅       │ ❌       │ ❌       │ ❌       │
│ Create orch  │ ✅       │ ❌       │ ❌       │ ❌       │
│ Edit settings│ ✅       │ ✅       │ ❌       │ ❌       │
│ View all orch│ ✅       │ ❌       │ ✅       │ ❌       │
│ Approve pay  │ ❌       │ ❌       │ ✅       │ ❌       │
│ Edit timesheet│ ✅      │ ✅       │ ✅       │ ❌       │
│ Add pickers  │ ❌       │ ✅       │ ❌       │ ✅       │
│ Delete users │ ✅       │ ❌       │ ❌       │ ❌       │
│ View audits  │ ✅       │ ✅       │ ✅       │ ❌       │
│ Export data  │ ✅       │ ✅       │ ✅       │ ❌       │
│ Send broadc. │ ✅       │ ✅       │ ❌       │ ❌       │
│ Scan buckets │ ❌       │ ❌       │ ❌       │ ✅ Runner │
│ Check-in crew│ ❌       │ ❌       │ ❌       │ ✅ TL    │
└──────────────┴──────────┴──────────┴──────────┴──────────┘
```

### Implementación sugerida

1. Tabla `permissions` en Supabase con columnas: `role`, `resource`, `action`, `allowed`
2. Hook `usePermissions()` que consulta los permisos del usuario actual
3. Componente `<PermissionGate permission="users.create">` que envuelve botones/secciones
4. RLS policies actualizadas para incluir verificación de permisos

---

## 9. FUNCIONES CRÍTICAS QUE FALTAN (independiente de roles)

| # | Función | Prioridad | Detalle |
|---|---------|-----------|---------|
| 1 | **Settings Page real** | 🔴 Alta | Manager no puede cambiar piece_rate, min_wage sin ir a Supabase directamente |
| 2 | **QC Inspector completo** | 🔴 Alta | Toda la página es cosmética. Necesita: picker selector, scan integration, DB writes |
| 3 | **CSV Import para Teams** | 🟡 Media | Botón existe, función no. Importar pickers desde CSV sería muy útil |
| 4 | **Transport Request (Runner)** | 🟡 Media | Botón existe, handler es undefined |
| 5 | **End of Day Report (TL)** | 🟡 Media | Botón visible sin handler |
| 6 | **Offline Mode toggle real** | 🟡 Media | Toggle visual pero sin efecto |
| 7 | **Truck/Fleet tracking** | 🟢 Baja | Datos de tractores hardcoded |
| 8 | **GPS-based HeatMap** | 🟢 Baja | Actual es list-based, no geospatial |
| 9 | **Photo capture en QC** | 🟢 Baja | Mencionado en "Coming Soon" banner |
| 10 | **Notifications real-time** | 🟡 Media | Bell icon exists, no notification system |

---

## 10. CONCLUSIÓN

### Lo bueno (Motor/Lógica)

- **36 services** en `/services` — backend layer completísimo
- Supabase con RLS multi-tenant real
- Offline-first con sync bridge + dead letter queue
- Payroll calculation con NZ compliance (NZST-aware, configurable rates — Round 3)
- Break compliance checks en NZST (L7)
- Live picker hours from `check_in_time` (L8)
- PaySauce/Xero export sin distorsión de horas (L6/L15)
- JWT auto-refresh via `supabase.functions.invoke` (L3)
- Audit trail completo
- Messaging real-time funcional
- Attendance con correcciones + audit trail
- HeatMap con queries reales a Supabase

### Lo que necesita trabajo

1. **QC Inspector** → Reconstruir desde cero con funcionalidad real
2. **Settings tab** → Crear UI real para configuración de orchard
3. **HR/Admin role** → Nuevo rol con gestión de usuarios y permisos
4. **Payroll Admin** → Darle su propia pantalla (actualmente → Manager)
5. **Permission system** → Implementar matriz de permisos granular
6. **Botones muertos** → Conectar o eliminar todos los buttons sin handler
