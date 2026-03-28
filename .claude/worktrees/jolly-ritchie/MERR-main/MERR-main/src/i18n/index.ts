/**
 * i18n — Lightweight Internationalization for HarvestPro NZ
 *
 * Supports 3 locales:
 *   - en (English) — default
 *   - es (Español) — for Spanish-speaking workers
 *   - mi (Te Reo Māori) — for NZ indigenous language support
 *
 * Uses React Context for simplicity — no external dependencies needed.
 * Translation keys follow a namespace.key pattern for organization.
 */

import React, { createContext, useContext, useState, useCallback, useMemo } from 'react';

export type Locale = 'en' | 'es' | 'mi';

export interface LocaleInfo {
    code: Locale;
    label: string;
    nativeName: string;
    flag: string;
}

export const SUPPORTED_LOCALES: LocaleInfo[] = [
    { code: 'en', label: 'English', nativeName: 'English', flag: '🇳🇿' },
    { code: 'es', label: 'Spanish', nativeName: 'Español', flag: '🇪🇸' },
    { code: 'mi', label: 'Māori', nativeName: 'Te Reo Māori', flag: '🇳🇿' },
];

// ─── Translation dictionaries ────────────────────────────────
type TranslationDict = Record<string, string>;

const translations: Record<Locale, TranslationDict> = {
    en: {
        // ── Navigation ──
        'nav.dashboard': 'Dashboard',
        'nav.teams': 'Teams',
        'nav.orchard_map': 'Orchard Map',
        'nav.logistics': 'Logistics',
        'nav.insights': 'Insights',
        'nav.messaging': 'Messaging',
        'nav.timesheet': 'Timesheet',
        'nav.settings': 'Settings',
        'nav.sync_errors': 'Sync Errors',
        'nav.more': 'More',

        // ── Dashboard ──
        'dashboard.title': 'Orchard Overview',
        'dashboard.live_monitoring': 'Live monitoring',
        'dashboard.velocity': 'Velocity',
        'dashboard.production': 'Production',
        'dashboard.est_cost': 'Est. Cost',
        'dashboard.active_crew': 'Active Crew',
        'dashboard.buckets': 'buckets',
        'dashboard.pickers': 'pickers',
        'dashboard.daily_target': 'Daily Target',
        'dashboard.complete': 'Complete',
        'dashboard.remaining': 'remaining',
        'dashboard.export': 'Export',
        'dashboard.live_map': 'Live Map',
        'dashboard.broadcast': 'Broadcast',

        // ── Fraud Shield ──
        'fraud.title': 'Fraud Shield',
        'fraud.live': 'Live',
        'fraud.demo': 'Demo',
        'fraud.server_side': 'Server-side detection — real-time analysis of scan patterns',
        'fraud.intelligent': 'Intelligent detection — understands real orchard workflows',
        'fraud.high': 'High',
        'fraud.medium': 'Medium',
        'fraud.low': 'Low',
        'fraud.dismissed': 'Dismissed',
        'fraud.no_anomalies': 'No anomalies detected',
        'fraud.normal_patterns': 'All scan patterns look normal for this filter',
        'fraud.all_flags': 'All Flags',
        'fraud.impossible_rate': 'Impossible Rate',
        'fraud.post_pickup': 'Post-Pickup',
        'fraud.peer_outlier': 'Peer Outlier',
        'fraud.off_hours': 'Off Hours',
        'fraud.duplicates': 'Duplicates',
        'fraud.inspect_profile': 'Inspect Profile & History',
        'fraud.smart_dismissals': 'Smart Dismissals',
        'fraud.scenarios_ignored': 'scenarios correctly ignored — the system understands your orchard',
        'fraud.analyzing': 'Analyzing scan patterns…',
        'fraud.refresh': 'Refresh anomalies',
        'fraud.rule_elapsed': 'Rule 1: Elapsed Time',
        'fraud.rule_peer': 'Rule 2: Peer Check',
        'fraud.rule_grace': 'Rule 3: Grace Period',
        'fraud.rule_elapsed_desc': 'Measures buckets ÷ time since last collection. Accumulated buckets under trees = normal. Impossible after-pickup spike = alert.',
        'fraud.rule_peer_desc': 'Compares each picker to their row mates. If everyone is fast = good tree. If ONLY one person is racing = suspicious.',
        'fraud.rule_grace_desc': 'First 90 min = warmup. Ladders, cold fruit, no tractors yet. System observes silently, only flags impossible velocity.',

        // ── Timesheet ──
        'timesheet.title': 'Timesheet Editor',
        'timesheet.attendance': 'Attendance Records',
        'timesheet.hours': 'Hours',

        // ── Sync Errors ──
        'sync.title': 'Sync Errors',
        'sync.dead_letter': 'Dead Letter Queue',
        'sync.all_clear': 'All Clear!',
        'sync.no_errors': 'No sync errors detected',

        // ── Settings ──
        'settings.title': 'Settings',
        'settings.language': 'Language',
        'settings.language_desc': 'Choose your preferred language',
        'settings.notifications': 'Notifications',
        'settings.profile': 'Profile',
        'settings.security': 'Security',
        'settings.about': 'About',

        // ── Common ──
        'common.loading': 'Loading…',
        'common.error': 'Something went wrong',
        'common.retry': 'Retry',
        'common.cancel': 'Cancel',
        'common.save': 'Save',
        'common.close': 'Close',
        'common.search': 'Search',
        'common.filter': 'Filter',
        'common.today': 'Today',
        'common.yesterday': 'Yesterday',
        'common.sign_out': 'Sign Out',

        // ── Scanner ──
        'scanner.scan': 'Scan',
        'scanner.manual_entry': 'Enter code manually',
        'scanner.camera_unavailable': 'Camera unavailable',
        'scanner.native': 'NATIVE',
        'scanner.hardware_accelerated': 'Using hardware-accelerated scanner',
        'scanner.align_qr': 'Align QR code within frame',
        'scanner.submit_code': 'Submit Code',
        'scanner.scanned': 'Scanned ✓',
        'scanner.code_registered': 'Code registered successfully',
        'scanner.scan_again': 'Scan Again',
        'scanner.switch_camera': 'Switch to Camera',
        'scanner.problem_scanning': 'Problem scanning? Enter code manually',

        // ── Auth ──
        'auth.welcome': 'Welcome back!',
        'auth.sign_in': 'Sign In',
        'auth.sign_in_desc': 'Sign in to access your dashboard',
        'auth.register': 'Register',
        'auth.email': 'Email',
        'auth.password': 'Password',
        'auth.forgot_password': 'Forgot your password?',
    },

    es: {
        // ── Navegación ──
        'nav.dashboard': 'Panel',
        'nav.teams': 'Equipos',
        'nav.orchard_map': 'Mapa del Huerto',
        'nav.logistics': 'Logística',
        'nav.insights': 'Análisis',
        'nav.messaging': 'Mensajes',
        'nav.timesheet': 'Planilla',
        'nav.settings': 'Ajustes',
        'nav.sync_errors': 'Errores de Sync',
        'nav.more': 'Más',

        // ── Panel ──
        'dashboard.title': 'Vista del Huerto',
        'dashboard.live_monitoring': 'Monitoreo en vivo',
        'dashboard.velocity': 'Velocidad',
        'dashboard.production': 'Producción',
        'dashboard.est_cost': 'Costo Est.',
        'dashboard.active_crew': 'Equipo Activo',
        'dashboard.buckets': 'cubetas',
        'dashboard.pickers': 'recolectores',
        'dashboard.daily_target': 'Meta Diaria',
        'dashboard.complete': 'Completo',
        'dashboard.remaining': 'restante',
        'dashboard.export': 'Exportar',
        'dashboard.live_map': 'Mapa en Vivo',
        'dashboard.broadcast': 'Transmitir',

        // ── Escudo Anti-Fraude ──
        'fraud.title': 'Escudo Anti-Fraude',
        'fraud.live': 'En Vivo',
        'fraud.demo': 'Demo',
        'fraud.server_side': 'Detección del servidor — análisis en tiempo real de patrones de escaneo',
        'fraud.intelligent': 'Detección inteligente — entiende flujos reales del huerto',
        'fraud.high': 'Alto',
        'fraud.medium': 'Medio',
        'fraud.low': 'Bajo',
        'fraud.dismissed': 'Descartados',
        'fraud.no_anomalies': 'No se detectaron anomalías',
        'fraud.normal_patterns': 'Todos los patrones de escaneo lucen normales',
        'fraud.all_flags': 'Todas',
        'fraud.impossible_rate': 'Tasa Imposible',
        'fraud.post_pickup': 'Post-Recolección',
        'fraud.peer_outlier': 'Atípico entre Pares',
        'fraud.off_hours': 'Fuera de Horario',
        'fraud.duplicates': 'Duplicados',
        'fraud.inspect_profile': 'Ver Perfil e Historial',
        'fraud.smart_dismissals': 'Descartes Inteligentes',
        'fraud.scenarios_ignored': 'escenarios ignorados correctamente — el sistema entiende tu huerto',
        'fraud.analyzing': 'Analizando patrones de escaneo…',
        'fraud.refresh': 'Actualizar anomalías',
        'fraud.rule_elapsed': 'Regla 1: Tiempo Transcurrido',
        'fraud.rule_peer': 'Regla 2: Comparación entre Pares',
        'fraud.rule_grace': 'Regla 3: Periodo de Gracia',
        'fraud.rule_elapsed_desc': 'Mide cubetas ÷ tiempo desde última recolección. Cubetas acumuladas bajo árboles = normal. Pico imposible post-recolección = alerta.',
        'fraud.rule_peer_desc': 'Compara a cada recolector con sus compañeros de fila. Si todos son rápidos = buen árbol. Si SOLO una persona va rápido = sospechoso.',
        'fraud.rule_grace_desc': 'Primeros 90 min = calentamiento. Escaleras, fruta fría, sin tractores aún. El sistema observa silenciosamente.',

        // ── Planilla ──
        'timesheet.title': 'Editor de Planilla',
        'timesheet.attendance': 'Registros de Asistencia',
        'timesheet.hours': 'Horas',

        // ── Errores de Sincronización ──
        'sync.title': 'Errores de Sincronización',
        'sync.dead_letter': 'Cola de Pendientes',
        'sync.all_clear': '¡Todo Limpio!',
        'sync.no_errors': 'No se detectaron errores de sincronización',

        // ── Ajustes ──
        'settings.title': 'Ajustes',
        'settings.language': 'Idioma',
        'settings.language_desc': 'Elige tu idioma preferido',
        'settings.notifications': 'Notificaciones',
        'settings.profile': 'Perfil',
        'settings.security': 'Seguridad',
        'settings.about': 'Acerca de',

        // ── Comunes ──
        'common.loading': 'Cargando…',
        'common.error': 'Algo salió mal',
        'common.retry': 'Reintentar',
        'common.cancel': 'Cancelar',
        'common.save': 'Guardar',
        'common.close': 'Cerrar',
        'common.search': 'Buscar',
        'common.filter': 'Filtrar',
        'common.today': 'Hoy',
        'common.yesterday': 'Ayer',
        'common.sign_out': 'Cerrar Sesión',

        // ── Escáner ──
        'scanner.scan': 'Escanear',
        'scanner.manual_entry': 'Ingresar código manualmente',
        'scanner.camera_unavailable': 'Cámara no disponible',
        'scanner.native': 'NATIVO',
        'scanner.hardware_accelerated': 'Usando escáner acelerado por hardware',
        'scanner.align_qr': 'Alinea el código QR en el marco',
        'scanner.submit_code': 'Enviar Código',
        'scanner.scanned': 'Escaneado ✓',
        'scanner.code_registered': 'Código registrado exitosamente',
        'scanner.scan_again': 'Escanear de Nuevo',
        'scanner.switch_camera': 'Cambiar a Cámara',
        'scanner.problem_scanning': '¿Problema escaneando? Ingresa el código manualmente',

        // ── Autenticación ──
        'auth.welcome': '¡Bienvenido!',
        'auth.sign_in': 'Iniciar Sesión',
        'auth.sign_in_desc': 'Inicia sesión para acceder a tu panel',
        'auth.register': 'Registrarse',
        'auth.email': 'Correo Electrónico',
        'auth.password': 'Contraseña',
        'auth.forgot_password': '¿Olvidaste tu contraseña?',
    },

    mi: {
        // ── Whakatere (Navigation) ──
        'nav.dashboard': 'Papatohu',
        'nav.teams': 'Ngā Rōpū',
        'nav.orchard_map': 'Mahere Māra',
        'nav.logistics': 'Whakahaere Rawa',
        'nav.insights': 'Ngā Tirohanga',
        'nav.messaging': 'Ngā Karere',
        'nav.timesheet': 'Rēhita Wā',
        'nav.settings': 'Ngā Tautuhinga',
        'nav.sync_errors': 'Hapa Tukutahi',
        'nav.more': 'Ētahi Atu',

        // ── Papatohu (Dashboard) ──
        'dashboard.title': 'Tirohanga Māra',
        'dashboard.live_monitoring': 'Aroturuki Ora',
        'dashboard.velocity': 'Tere',
        'dashboard.production': 'Whakaputa',
        'dashboard.est_cost': 'Utu Whakatau',
        'dashboard.active_crew': 'Rōpū Hohe',
        'dashboard.buckets': 'ngā pākete',
        'dashboard.pickers': 'ngā kaikohi',
        'dashboard.daily_target': 'Whāinga o te Rā',
        'dashboard.complete': 'Oti',
        'dashboard.remaining': 'e toe ana',
        'dashboard.export': 'Tuku ki Waho',
        'dashboard.live_map': 'Mahere Ora',
        'dashboard.broadcast': 'Pāpāho',

        // ── Puāwai Anti-Fraude ──
        'fraud.title': 'Puāwai Haumaru',
        'fraud.live': 'Ora',
        'fraud.demo': 'Whakaatu',
        'fraud.server_side': 'Kitenga tūmau — tātaritanga wā-tūturu',
        'fraud.intelligent': 'Kitenga atamai — mārama ki ngā ritenga māra',
        'fraud.high': 'Teitei',
        'fraud.medium': 'Waenga',
        'fraud.low': 'Iti',
        'fraud.dismissed': 'Whakakore',
        'fraud.no_anomalies': 'Kāore he rerekētanga i kitea',
        'fraud.normal_patterns': 'He pai ngā tauira katoa',
        'fraud.all_flags': 'Katoa',
        'fraud.analyzing': 'E tātari ana i ngā tauira…',
        'fraud.refresh': 'Whakahou',

        // ── Ngā Tautuhinga (Settings) ──
        'settings.title': 'Ngā Tautuhinga',
        'settings.language': 'Reo',
        'settings.language_desc': 'Kōwhiria tō reo',

        // ── Noa (Common) ──
        'common.loading': 'E uta ana…',
        'common.error': 'He hapa',
        'common.retry': 'Ngana anō',
        'common.cancel': 'Whakakore',
        'common.save': 'Tiaki',
        'common.close': 'Kati',
        'common.search': 'Rapu',
        'common.filter': 'Tātari',
        'common.today': 'I tēnei rā',
        'common.yesterday': 'Inanahi',
        'common.sign_out': 'Takiputa',

        // ── Motuhēhē (Auth) ──
        'auth.welcome': 'Nau mai!',
        'auth.sign_in': 'Takiuru',
        'auth.sign_in_desc': 'Takiuru ki tō papatohu',
        'auth.register': 'Rēhita',
        'auth.email': 'Īmēra',
        'auth.password': 'Kupuhipa',
        'auth.forgot_password': 'Kua wareware tō kupuhipa?',
    },
};

// ─── Context ──────────────────────────────────────────────────

interface I18nContextValue {
    locale: Locale;
    setLocale: (locale: Locale) => void;
    t: (key: string) => string;
    localeInfo: LocaleInfo;
}

const I18N_STORAGE_KEY = 'harvestpro_locale';

function getInitialLocale(): Locale {
    try {
        const stored = localStorage.getItem(I18N_STORAGE_KEY) as Locale | null;
        if (stored && translations[stored]) return stored;
    } catch { /* Ignore */ }

    // Auto-detect from browser
    const browserLang = navigator.language?.split('-')[0];
    if (browserLang === 'es') return 'es';
    if (browserLang === 'mi') return 'mi';
    return 'en';
}

const I18nContext = createContext<I18nContextValue>({
    locale: 'en',
    setLocale: () => { },
    t: (key: string) => key,
    localeInfo: SUPPORTED_LOCALES[0],
});

export const I18nProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const [locale, setLocaleState] = useState<Locale>(getInitialLocale);

    const setLocale = useCallback((newLocale: Locale) => {
        setLocaleState(newLocale);
        try {
            localStorage.setItem(I18N_STORAGE_KEY, newLocale);
        } catch { /* Ignore */ }
    }, []);

    const t = useCallback((key: string): string => {
        return translations[locale][key] || translations['en'][key] || key;
    }, [locale]);

    const localeInfo = useMemo(
        () => SUPPORTED_LOCALES.find(l => l.code === locale) || SUPPORTED_LOCALES[0],
        [locale]
    );

    const value = useMemo(() => ({ locale, setLocale, t, localeInfo }), [locale, setLocale, t, localeInfo]);

    return React.createElement(I18nContext.Provider, { value }, children);
};

export function useI18n() {
    return useContext(I18nContext);
}

export function useTranslation() {
    const { t, locale, setLocale, localeInfo } = useI18n();
    return { t, locale, setLocale, localeInfo };
}
