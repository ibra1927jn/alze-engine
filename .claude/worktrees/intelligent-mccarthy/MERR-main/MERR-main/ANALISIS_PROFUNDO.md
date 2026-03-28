# Analisis Profundo -- alze os
**Fecha:** 2026-03-27

**Proyecto:** HarvestPro NZ (nombre interno: `harvestpro-nz`, version 9.0.0)
**Descripcion:** Plataforma industrial de gestion de cosecha para huertos en Nueva Zelanda. PWA offline-first con 8 roles, escaneo QR, nomina con Wage Shield, y sincronizacion delta.
**Tamano estimado:** ~35k LOC (segun README), ~300 archivos fuente

---

## 1. Modulos/Sistemas Completamente Implementados

### 1.1 Sistema de Autenticacion y Roles (COMPLETO)
- **8 roles definidos:** Manager, Team Leader, Runner, QC Inspector, Payroll Admin, Admin, HR Admin, Logistics
- `AuthContext.tsx` completo con: login/logout, carga de perfil desde Supabase, reautenticacion (ReAuthModal), tracking Sentry/PostHog, cache offline via Dexie
- Rutas protegidas con `ProtectedRoute` que verifica roles y redirige automaticamente (`routes.tsx`)
- Code splitting con `React.lazy` para cada pagina
- MFA implementado: `MFAGuard.tsx`, `MFASetup.tsx`, `MFAVerify.tsx`, hook `useMFA.ts`
- Auth hardening service (`authHardening.service.ts`) con tests

### 1.2 Manager Dashboard (COMPLETO - modulo mas maduro)
- Vista adaptativa desktop/mobile con `DesktopLayout` + `BottomNav`
- **Sub-vistas implementadas (14+):** DashboardView, TeamsView, LogisticsView, MessagingView, MapToggleView, InsightsView, SettingsView, MoreMenuView, OrchardMapView, HeatMapView, LiveFloor, VelocityChart, WageShieldPanel, WeeklyReportView, CostAnalyticsView, AnomalyDetectionView, DayClosureButton, PerformanceFocus, TimesheetEditor, DeadLetterQueueView, RowListView
- Modals completos: DaySettingsModal, AddPickerModal, BroadcastModal, RowAssignmentModal, PickerDetailsModal
- Navegacion configurada en `managerNav.config.ts` con tabs mobile y nav desktop

### 1.3 Sistema de Escaneo y Bucket Ledger (COMPLETO)
- `bucket-ledger.service.ts`: registro inmutable de cada bucket con validacion Zod
- Resolucion de badge ID a UUID de picker (match exacto, sin fuzzy por seguridad financiera)
- `ScannerModal.tsx` con lazy-load de html5-qrcode (~250KB)
- Soporte para escaneo QR y codigos de sticker
- Esquema de validacion via `@/lib/schemas.ts` con Zod
- Rate limiting de escaneo (`useScanRateLimit.ts`)
- Servicio de stickers (`sticker.service.ts`)

### 1.4 Sincronizacion Offline-First (COMPLETO)
- **IndexedDB via Dexie** con 8+ tablas: bucket_queue, sync_queue, dead_letter_queue, sync_meta, conflicts, cached_users, cached_settings, message_queue
- `sync.service.ts`: cola unificada con Web Locks API para mutex cross-tab
- `offline.service.ts`: queue de buckets con cleanup automatico (7 dias)
- `HarvestSyncBridge.tsx`: polling 5s-5min, batch insert a Supabase, backoff exponencial, manejo de duplicados (error 23505)
- **Delta sync** implementado: full fetch si > 24h, delta fetch con ventana de 2min overlap
- Procesadores de sync modulares en `sync-processors/`: attendance, contract, timesheet, transport
- Dead Letter Queue (DLQ) con 50 reintentos max
- Conflict resolution service (`conflict.service.ts`) con UI `ConflictResolver.tsx`
- Optimistic locking (`optimistic-lock.service.ts`)

### 1.5 Zustand Store (COMPLETO - bien arquitectado)
- Store principal `useHarvestStore.ts` como orquestador slim (~112 lineas)
- **7 slices de dominio:** settingsSlice, crewSlice, bucketSlice, intelligenceSlice, rowSlice, orchardMapSlice, uiSlice
- Persistencia via `safeStorage.ts` (localStorage con manejo de cuota)
- Hidratacion desde Dexie y recovery
- Suscripciones realtime de Supabase con auto-desconexion cuando tab hidden
- Cada slice tiene su test unitario

### 1.6 Control de Calidad (QC) (COMPLETO)
- Pagina `QualityControl.tsx` con 4 tabs: Inspect, History, Stats, Trends
- Servicio `qc.service.ts` con CRUD de inspecciones y distribucion de grados (A/B/C/reject)
- Subida de fotos a Supabase Storage (bucket `qc-photos`)
- Componentes visuales: `DistributionBar.tsx`, `TrendsTab.tsx`
- Historial de inspecciones con modal `InspectionHistoryModal.tsx`

### 1.7 Sistema de Mensajeria (COMPLETO)
- `MessagingContext.tsx` con soporte para: direct messages, team messages, broadcasts
- `simple-messaging.service.ts` con repositorio dedicado
- Componentes: `ChatWindow.tsx`, `MessagingSidebar.tsx`, `NewChatModal.tsx`, `SendDirectMessageModal.tsx`, `BroadcastModal.tsx`
- Vistas de mensajeria especificas por rol (manager, team-leader, runner)
- Unified messaging view (`UnifiedMessagingView.tsx`)

### 1.8 Payroll / Nomina (COMPLETO)
- Pagina `Payroll.tsx` con 5 tabs: Dashboard, Timesheets, Wage Calculator, Export, History
- Servicio `payroll.service.ts` con llamada a Edge Function de Supabase para calculos server-side
- **Wage Shield** implementado: deteccion de workers bajo salario minimo NZ ($23.50/hr), calculo de top-up
- Exportacion multi-formato: CSV generico, Xero, PaySauce (`export-payroll-formats.service.ts`)
- Template PDF (`export-pdf-template.service.ts`)
- Sanitizacion de CSV contra formula injection

### 1.9 Team Leader (COMPLETO)
- 7 tabs: Home, Attendance/Roll Call, Team, Tasks, Timesheet, Profile, Chat
- Vistas dedicadas: `HomeView`, `AttendanceView`, `TeamView`, `TasksView`, `ProfileView`, `MessagingView`
- Integracion con TimesheetEditor compartido

### 1.10 Runner / Bucket Runner (COMPLETO)
- 5 tabs: Logistics, Runners, Warehouse, Messaging, Timesheet
- Escaneo QR con `ScannerModal` (lazy-loaded)
- Feedback haptilo via `feedbackService.vibrate()`
- Monitor de sincronizacion (`SyncStatusMonitor.tsx`)
- Quality assessment workflow post-scan

### 1.11 RRHH (HR Admin) (COMPLETO)
- 6 tabs: Employees, Contracts, Payroll, Documents, Calendar, Seasonal Planning
- Servicios: `hhrr.service.ts` con Employee, Contract, PayrollEntry, ComplianceAlert
- Gestion de contratos (permanent/seasonal/casual)
- Alertas de compliance (visas expiradas, contratos por vencer)

### 1.12 Logistica (COMPLETO)
- 5 tabs: Fleet, Bin Inventory, Requests, Routes, History
- Servicio `logistics-dept.service.ts` con: FleetTab, BinsTab, RequestsTab, RoutesTab, HistoryTab
- Realtime subscriptions para transport_requests y fleet_vehicles
- Request dispatch y transport history

### 1.13 Admin (COMPLETO)
- 4 tabs: Orchards, Users, Compliance, Audit Log
- Servicio `admin.service.ts` con CRUD multi-orchard
- `AuditLogViewer.tsx` para trail de auditoria
- Setup wizard para onboarding inicial

### 1.14 i18n - Internacionalizacion (COMPLETO)
- 4 idiomas: English, Spanish (Espanol), Samoan, Tongan
- Archivos de traducciones en `services/translations/`
- `I18nProvider` en `i18n/index.ts`
- Selector de idioma (`LanguageSelector.tsx`) con test
- Persistencia de preferencia en localStorage

### 1.15 Compliance NZ (COMPLETO)
- `compliance.service.ts` con reglas de Employment Relations Act:
  - Descansos: 10min cada 2h, 30min cada 4h
  - Salario minimo: $23.50/hr
  - Horas maximas consecutivas
- Tipos de violacion: break_overdue, wage_below_minimum, excessive_hours, hydration_reminder

### 1.16 Deteccion de Fraude (COMPLETO pero delegado a backend)
- `fraud-detection.service.ts` como puente frontend -> Edge Function `detect-anomalies`
- 5 tipos de anomalia: impossible_velocity, peer_outlier, off_hours, duplicate_proximity, post_collection_spike
- Configuracion: grace period 90min, peer threshold 3.0, max rate 8 bkt/hr
- Fallback a mock data cuando offline

### 1.17 Repositorios (COMPLETO - capa limpia)
- **22 repositorios** en `src/repositories/`: admin, analyticsTrends, attendance, audit, auth, authContext, baseRepository, bin, bucketEvents, bucketLedger, contract, edgeFunctions, logistics, messaging, optimisticLock, picker, pickerCrud, qc, row, rpc, settings, setup, sticker, storeSync, user, userService
- Tests para los repositorios criticos

### 1.18 PWA (COMPLETO)
- `vite-plugin-pwa` con autoUpdate, workbox, manifest completo
- Runtime caching: NetworkFirst para Supabase API, StaleWhileRevalidate para Google Fonts
- Manual chunks optimizados: vendor-react, vendor-supabase, vendor-state, vendor-monitoring
- `PwaInstallBanner.tsx` con lazy-load
- Persistent storage request para prevenir eviccion en iOS Safari
- Offline page (`public/offline.html`)

### 1.19 Observabilidad (COMPLETO)
- **Sentry** para error tracking con filtros (auth errors, network errors excluidos)
- **PostHog** para analytics de producto (eventos: bucket_scanned, user_login, offline_sync, broadcast_sent, dlq_error)
- Logger centralizado (`utils/logger.ts`)
- Privacy: autocapture desactivado, cookies eliminadas de reports

---

## 2. Modulos a Medias o Estructura Vacia

### 2.1 Capacitor / App Nativa (ESTRUCTURA BASICA SOLAMENTE)
- `capacitor.config.ts` configurado con plugins (BarcodeScanner, Camera, SplashScreen, StatusBar)
- Tipo declaration para `capacitor-barcode-scanner.d.ts`
- `native-scanner.service.ts` existe pero la app NO tiene directorio `android/` ni `ios/`
- **NO se ha ejecutado `npx cap add android/ios`** - la configuracion esta lista pero la compilacion nativa nunca se ha hecho

### 2.2 Dark Mode (PREPARADO PERO NO IMPLEMENTADO)
- Tailwind config tiene `darkMode: "class"`
- Feature flag preparado: `VITE_FEATURE_DARK_MODE=false`
- Ningun componente implementa variantes dark (`dark:bg-...`)

### 2.3 Push Notifications (SOLO FEATURE FLAG)
- Feature flag: `VITE_FEATURE_PUSH_NOTIFICATIONS=false`
- `notification.service.ts` existe con polling basico, pero no hay implementacion de Web Push, ni service worker push handler, ni integracion con FCM/APNs

### 2.4 Gemini AI Integration (SOLO KEY EN .env)
- `VITE_GEMINI_API_KEY` definido en .env.example
- No se encontro ningun servicio que consuma la API de Gemini
- Probablemente planeado para inteligencia predictiva o recomendaciones

### 2.5 Mapa de Huerto Real (MOCK/SIMULACION)
- `OrchardMapView.tsx`, `BlockCard.tsx`, `RowCard.tsx`, `ProgressRing.tsx` implementados
- Los bloques y filas se modelan con interfaces (`OrchardBlock`, `RowAssignment`)
- **NO hay integracion con mapas reales** (Google Maps, Mapbox, Leaflet) - todo es representacion abstracta/grid
- No hay coordenadas GPS reales ni geocoding

### 2.6 Playwright E2E Tests (CONFIGURADOS PERO ESTADO INCIERTO)
- 17 archivos de spec en `tests/e2e/`: login-flows, offline-sync, payroll-accuracy, conflict-resolver, rls-cross-tenant, etc.
- 3 configs: base, staging, production
- CI los ejecuta solo en staging/main branches
- No se puede verificar si pasan actualmente sin ejecutarlos

### 2.7 Coverage Threshold (CONFIGURADO PERO CONTINUE-ON-ERROR)
- Deploy production verifica coverage >= 70% pero con `continue-on-error: true`
- Es decir, el deploy no falla si el coverage esta por debajo del umbral

### 2.8 Orchard Map con GPS/GeoJSON (NO EXISTE)
- El tipo `BucketRecord` tiene campo `coords?: { lat: number; lng: number }` pero es opcional
- No hay servicio de geolocalizacion ni integracion con APIs de mapas
- El HeatMapView probablemente usa datos simulados

---

## 3. Problemas Tecnicos a Primera Vista

### 3.1 Credenciales y Seguridad
- `config.service.ts` tiene comentario `WARNING: CREDENTIALS REMOVED` indicando que previamente hubo credenciales hardcodeadas
- `APP_VERSION` fallback es `'4.2.0'` en config.service.ts pero package.json dice `9.0.0` - inconsistencia
- El `tabId` para aislamiento de sesion usa `Date.now()` + `Math.random()` en vez de `crypto.randomUUID()` - menor entropia

### 3.2 Supabase Client - Aislamiento por Tab Potencialmente Problematico
- Cada tab genera su propio `storageKey` para auth, lo que significa que cerrar/abrir tabs puede resultar en sesiones desincronizadas
- Si un usuario abre 3 tabs, tiene 3 sesiones auth independientes - esto puede causar race conditions en token refresh

### 3.3 Archivos de Test de Resultado Commiteados
- `scripts/rls_results.txt`, `rls_results_after_fix.txt`, `rls_results_final.txt`, `rls_test_output.txt`, `concurrency_results.txt` estan en el repositorio - deberian estar en `.gitignore`
- `dash-test.txt`, `dash-test2.txt`, `dash-test3.txt` en raiz - archivos temporales de debug
- `push_output.txt` - output de git push commiteado

### 3.4 Dependencia de Edge Functions No Incluidas
- `fraud-detection.service.ts` llama a Edge Function `detect-anomalies`
- `payroll.service.ts` usa Edge Functions para calculos de nomina
- Estas Edge Functions NO estan en el repositorio - estan desplegadas directamente en Supabase
- Si se pierde acceso al proyecto Supabase, se pierde logica de negocio critica

### 3.5 ESLint Warnings Suprimidos
- Multiples archivos usan `/* eslint-disable react-refresh/only-export-components */`
- Patron `// eslint-disable-next-line @typescript-eslint/no-explicit-any` frecuente en sentry.ts

### 3.6 Tailwind Config Excesivo
- El `content` de tailwind incluye paths que no existen a nivel raiz: `./pages/**`, `./components/**`, `./hooks/**`, `./context/**`, `./services/**` - todo esta dentro de `./src/`

### 3.7 Tipado Parcial de Database
- `src/types/database.types.ts` existe pero se importan tipos de el selectivamente
- Los tipos de Supabase no parecen auto-generados con `supabase gen types` - probablemente manuales

### 3.8 Version de TypeScript Anticuada en package.json
- `typescript: ^5.3.3` declarado, pero React 19 y las dependencias mas nuevas ya usan TS 5.5+
- Los ESLint plugins (`@typescript-eslint/*: ^6.21.0`) son bastante viejos - ya existe v8+

### 3.9 Migraciones SQL Sin Orden Claro
- Solo 1 archivo numerado: `03_secure_rls.sql`
- El resto no sigue convencion: `audit_logs.sql`, `bucket_ledger.sql`, `fix_runtime_errors.sql`, etc.
- `apply_all_migrations.sql` probablemente concatena todo manualmente
- No hay herramienta de migraciones (supabase migrations, knex, etc.)

### 3.10 Result Type Infrautilizado
- `src/types/result.ts` define un tipo Result pero muchos servicios todavia usan throw/catch en lugar de Result patterns

### 3.11 Simulacion Mode Sin Guardia Completa
- El store tiene `simulationMode` toggle pero no se verifica consistentemente en todos los servicios que escriben datos
- Un manager podria olvidar que esta en modo simulacion y enviar datos de test a produccion

---

## 4. Librerias y Herramientas Exactas

### Framework y UI
| Libreria | Version | Proposito |
|----------|---------|-----------|
| React | ^19.2.3 | Framework UI |
| React DOM | ^19.2.3 | Renderizado DOM |
| React Router DOM | ^7.13.0 | Enrutamiento SPA |
| Tailwind CSS | ^3.4.0 | Estilos utility-first |
| @tailwindcss/forms | ^0.5.11 | Plugin formularios |
| lucide-react | ^0.563.0 | Iconos SVG |
| clsx | ^2.1.1 | Condicionales de clases |
| tailwind-merge | ^3.5.0 | Merge inteligente de clases TW |

### State Management y Datos
| Libreria | Version | Proposito |
|----------|---------|-----------|
| zustand | ^5.0.11 | State management global |
| @supabase/supabase-js | ^2.39.0 | Cliente Supabase (auth, DB, realtime, storage) |
| dexie | ^3.2.4 | IndexedDB wrapper (offline-first) |
| @tanstack/react-query | ^5.90.21 | Server state / caching |
| zod | ^4.3.6 | Validacion de schemas |

### Funcionalidades Especificas
| Libreria | Version | Proposito |
|----------|---------|-----------|
| html5-qrcode | ^2.3.8 | Escaneo QR/barcode |
| papaparse | ^5.5.3 | Parseo CSV |
| date-fns | ^4.1.0 | Manipulacion de fechas |
| react-virtuoso | ^4.18.1 | Listas virtualizadas (performance) |

### Observabilidad
| Libreria | Version | Proposito |
|----------|---------|-----------|
| @sentry/react | ^10.39.0 | Error tracking |
| posthog-js | ^1.345.3 | Product analytics |

### Build y Dev
| Herramienta | Version | Proposito |
|-------------|---------|-----------|
| Vite | ^7.3.1 | Bundler |
| @vitejs/plugin-react | ^5.1.2 | Plugin React para Vite |
| vite-plugin-pwa | ^1.2.0 | Service Worker / PWA |
| TypeScript | ^5.3.3 | Tipado estatico |
| ESLint | ^8.57.0 | Linting |
| Prettier | ^3.2.5 | Formateo |
| PostCSS | ^8.4.32 | Procesamiento CSS |
| Autoprefixer | ^10.4.16 | Prefijos CSS |

### Testing
| Herramienta | Version | Proposito |
|-------------|---------|-----------|
| Vitest | ^4.0.18 | Unit testing |
| @testing-library/react | ^16.3.2 | Testing de componentes |
| @testing-library/jest-dom | ^6.9.1 | Matchers de DOM |
| @testing-library/user-event | ^14.6.1 | Simulacion de interacciones |
| @vitest/coverage-v8 | ^4.0.18 | Coverage con V8 |
| Playwright | ^1.58.2 | E2E testing |
| jsdom | ^24.0.0 | DOM simulado |
| fake-indexeddb | ^6.2.5 | IndexedDB para tests |
| sharp | ^0.34.5 | Generacion de iconos |

### Plataforma Nativa (configurado, no compilado)
| Herramienta | Proposito |
|-------------|-----------|
| Capacitor | Bridge web -> nativo (Android/iOS) |

### CI/CD
| Herramienta | Proposito |
|-------------|-----------|
| GitHub Actions | CI (lint, typecheck, test, build, security-scan, e2e) |
| Vercel | Deploy (staging + production) |
| Codecov | Coverage reporting |

### Backend (no en repo, via Supabase)
| Servicio | Proposito |
|----------|-----------|
| Supabase PostgreSQL | Base de datos con RLS |
| Supabase Auth | Autenticacion |
| Supabase Realtime | Suscripciones en tiempo real |
| Supabase Storage | Almacenamiento de fotos QC |
| Supabase Edge Functions | Calculos server-side (payroll, fraud detection) |

---

## 5. Ficheros Mas Importantes

### Arquitectura Core
- `src/routes.tsx` -- Enrutamiento completo con proteccion por roles
- `src/index.tsx` -- Entry point con providers (I18n, Auth, MFA, PWA)
- `src/types.ts` -- ~270 lineas con TODOS los tipos de dominio
- `src/stores/useHarvestStore.ts` -- Orquestador central del estado
- `src/stores/storeSync.ts` -- Hidratacion, delta sync, suscripciones realtime
- `src/services/supabase.ts` -- Cliente Supabase singleton con aislamiento por tab

### Servicios Criticos
- `src/services/sync.service.ts` -- Cola offline con Web Locks mutex
- `src/services/bucket-ledger.service.ts` -- Ledger financiero inmutable
- `src/services/offline.service.ts` -- Persistencia offline de buckets
- `src/services/compliance.service.ts` -- Reglas laborales NZ
- `src/services/payroll.service.ts` -- Calculos de nomina (via Edge Function)
- `src/services/fraud-detection.service.ts` -- Deteccion de anomalias
- `src/services/config.service.ts` -- Gestion segura de configuracion
- `src/services/attendance.service.ts` -- Check-in/out con RPC atomico
- `src/services/hhrr.service.ts` -- Gestion de empleados y contratos
- `src/services/logistics-dept.service.ts` -- Flota y transporte
- `src/services/db.ts` -- Schema Dexie (IndexedDB)
- `src/services/conflict.service.ts` -- Resolucion de conflictos offline

### Paginas Principales
- `src/pages/Manager.tsx` -- Dashboard del manager (mas complejo, ~300 lineas)
- `src/pages/TeamLeader.tsx` -- Vista team leader
- `src/pages/Runner.tsx` -- Escaneo y warehouse
- `src/pages/QualityControl.tsx` -- Inspecciones de calidad
- `src/pages/Payroll.tsx` -- Nomina
- `src/pages/HHRR.tsx` -- Recursos humanos
- `src/pages/LogisticsDept.tsx` -- Logistica
- `src/pages/Admin.tsx` -- Administracion del sistema

### Configuracion
- `package.json` -- Dependencias y scripts
- `vite.config.ts` -- Build con PWA, chunks, aliases
- `tailwind.config.js` -- Design system completo
- `.github/workflows/ci.yml` -- Pipeline CI de 6 jobs
- `.github/workflows/deploy-production.yml` -- Deploy a Vercel
- `capacitor.config.ts` -- Config nativa (no compilada)
- `.env.example` -- Variables de entorno requeridas

### SQL/Migraciones
- `sql/03_secure_rls.sql` -- Politicas RLS por orchard_id
- `sql/audit_logs.sql` -- Tabla de auditoria
- `sql/bucket_ledger.sql` -- Tabla del ledger financiero
- `scripts/apply_all_migrations.sql` -- Script de aplicacion

### Tests Clave
- `tests/e2e/*.spec.ts` -- 17 specs E2E con Playwright
- Cada servicio tiene su `.test.ts` colocado junto al archivo fuente
- Cada slice del store tiene tests unitarios

---

## 6. Lo Que Falta Para Ser un Producto Completo

### 6.1 CRITICO - Sin lo cual NO se puede usar en produccion real

1. **Edge Functions no versionadas en el repo** -- Las funciones de Supabase (payroll calc, fraud detection) no estan en el repositorio. Si se pierde el proyecto Supabase, se pierde logica de negocio critica. Deben guardarse en `supabase/functions/` y desplegarse via CI.

2. **Migraciones SQL sin sistema formal** -- No hay herramienta de migraciones. Los archivos SQL estan sueltos sin orden. Se necesita `supabase migrations` o Flyway/Knex para despliegues reproducibles.

3. **No hay backend propio** -- Todo depende de Supabase (BaaS). Si Supabase tiene downtime o cambia pricing, no hay alternativa. Considerar tener al menos un plan de contingencia.

4. **Tests E2E sin verificacion de estado** -- Los specs de Playwright existen pero no se sabe si pasan. El coverage threshold (`>= 70%`) tiene `continue-on-error`.

5. **Push Notifications no implementadas** -- Para workers en campo, las notificaciones push son esenciales (broadcast urgentes, alertas de compliance). Solo hay polling basico.

### 6.2 IMPORTANTE - Afecta UX y operaciones

6. **App nativa no compilada** -- Capacitor esta configurado pero nunca se ejecuto `cap add`. Los workers de campo necesitan una app instalable. La PWA funciona pero la experiencia nativa (camara, barcode, notificaciones) seria significativamente mejor.

7. **Dark mode no implementado** -- Preparado en config pero ningun componente lo usa. Importante para uso en campo con brillo bajo.

8. **Mapa real de huerto** -- Solo hay representacion abstracta de bloques y filas. Un mapa interactivo con GPS real mejoraria significativamente la operacion de campo.

9. **Reportes avanzados / BI** -- WeeklyReportView existe pero no hay exportacion a dashboard de BI. No hay integracion con herramientas de reporting externas.

10. **Rate limiting en API** -- No hay throttling visible del lado del cliente para prevenir abuso de API de Supabase (mas alla del scan rate limit).

### 6.3 DESEADO - Para un producto comercial completo

11. **AI/ML predictivo** -- La key de Gemini esta preparada pero no hay integracion. Prediccion de tonelaje, optimizacion de rutas, prediccion de rendimiento por picker serian features diferenciadoras.

12. **Calendario de turnos** -- HHRR tiene CalendarTab pero no hay scheduling real de shifts con drag-and-drop.

13. **Documents management** -- DocumentsTab existe en HHRR pero no se verifico si tiene upload/download de documentos reales (contratos PDF, visas, etc.).

14. **Audit trail inmutable** -- `AuditLogViewer` existe pero los logs de auditoria deberian ser append-only en la base de datos (no updateable/deleteable).

15. **Multi-tenant real** -- RLS por orchard_id funciona, pero no hay gestion de organizaciones/empresas. Un propietario con 5 huertos necesitaria crear 5 cuentas.

16. **Onboarding / Tutorial** -- SetupWizard existe para configuracion inicial pero no hay tutorial interactivo para nuevos usuarios en campo.

17. **Backup y recovery** -- No hay estrategia documentada de backup de datos. Si IndexedDB se corrompe en un dispositivo, no hay recovery path claro.

18. **Accesibilidad (a11y)** -- README menciona WCAG 2.1 pero no se encontraron tests de accesibilidad (axe, lighthouse a11y) ni atributos aria extensivos en componentes.

19. **Logging server-side** -- Solo hay logging del lado del cliente. No hay integracion con herramientas de log aggregation server-side.

20. **Documentacion de API** -- No hay documentacion de las tablas de Supabase, RPC functions, ni Edge Functions. Un nuevo desarrollador necesitaria reverse-enginear todo.

### 6.4 Deuda Tecnica Menor

21. Limpiar archivos temporales del repo (`dash-test*.txt`, `push_output.txt`, `*_results.txt`)
22. Actualizar TypeScript a 5.5+ y ESLint plugins a v8+
23. Corregir version fallback en config.service.ts (4.2.0 -> 9.0.0)
24. Corregir paths de Tailwind content para que apunten a `./src/`
25. Auto-generar tipos de base de datos con `supabase gen types`
26. Mover Edge Functions al repositorio bajo `supabase/functions/`

---

## Resumen Ejecutivo

**HarvestPro NZ es un proyecto sorprendentemente maduro y completo para un solo desarrollador.** Los 8 roles estan implementados con dashboards funcionales, el sistema offline-first es sofisticado (Dexie + Web Locks + DLQ + delta sync + conflict resolution), y la capa de servicios esta bien separada con repositorios, servicios, hooks y stores.

**Puntos fuertes:**
- Arquitectura offline-first de nivel produccion
- Compliance laboral NZ integrada en el core
- Sistema de escaneo QR con validacion financiera
- CI/CD completo con 6 jobs
- 22+ repositorios con patron limpio
- Observabilidad (Sentry + PostHog)

**Riesgos principales:**
- Edge Functions fuera del repo (vendor lock-in critico con Supabase)
- Sin migraciones formales (dificil reproducir schema)
- App nativa no compilada (field workers necesitan app real)
- Push notifications ausentes (critico para operaciones de campo)

**Estimacion de completitud:** ~80-85% para MVP de produccion. El 15-20% restante es mayoritariamente: Edge Functions versionadas, app nativa compilada, push notifications, y sistema formal de migraciones.
