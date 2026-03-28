/**
 * test-utils.tsx — Custom Test Render Factory
 * 
 * Provides a pre-configured `render` function that wraps components in all
 * required providers (AuthContext, HarvestStore, etc.), eliminating the need
 * to manually mock every transitive dependency in component tests.
 * 
 * Usage:
 *   import { render, screen } from '@/utils/test-utils';
 *   render(<Manager />);
 *   expect(screen.getByText('Dashboard')).toBeTruthy();
 * 
 * @module utils/test-utils
 */
import React, { ReactElement } from 'react';
import { render, RenderOptions } from '@testing-library/react';
import { vi } from 'vitest';

// ── Default mock values ─────────────────────────────
export const mockUser = {
    id: 'test-user-id',
    email: 'test@harvestpro.co.nz',
};

export const mockAppUser = {
    id: 'test-user-id',
    full_name: 'Test Manager',
    role: 'manager' as const,
    orchard_id: 'test-orchard-id',
    picker_id: 'P001',
    team_leader_id: null,
    status: 'active' as const,
    email: 'test@harvestpro.co.nz',
};

export const mockAuthContext = {
    user: mockUser,
    appUser: mockAppUser,
    isAuthenticated: true,
    isLoading: false,
    isSetupComplete: true,
    currentRole: 'manager' as const,
    userName: 'Test Manager',
    userEmail: 'test@harvestpro.co.nz',
    orchardId: 'test-orchard-id',
    teamId: null,
    needsReAuth: false,
    isDemoMode: false,
    signIn: vi.fn(),
    signUp: vi.fn(),
    signOut: vi.fn(),
    logout: vi.fn(),
    resetPassword: vi.fn(),
    completeSetup: vi.fn(),
    updateAuthState: vi.fn(),
};

export const mockHarvestState = {
    crew: [],
    buckets: [],
    bucketRecords: [],
    settings: { bucketRate: 3.0, targetTons: 10, min_buckets_per_hour: 3.6, piece_rate: 1.5, min_wage_rate: 23.15 },
    stats: { totalBuckets: 0, tons: 0, velocity: 0, goalVelocity: 0, payEstimate: 0, binsFull: 0 },
    inventory: [],
    rowAssignments: [],
    currentUser: { id: 'test-user-id', name: 'Test Manager', role: 'manager' },
    orchard: { id: 'test-orchard-id', name: 'Test Orchard', total_rows: 50 },
    simulationMode: false,
    dayClosed: false,
    notifications: [],
    alerts: [],
    payroll: { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
    blocks: [],
    presentCount: 0,
    isScanning: false,
    lastScanTime: null,
    recentQcInspections: [],
    recentTimesheetUpdates: [],
    fetchGlobalData: vi.fn(),
    recalculateIntelligence: vi.fn(),
    setGlobalState: vi.fn(),
    addBucket: vi.fn(),
    fetchBlocks: vi.fn(),
    setDayClosed: vi.fn(),
    openPickerProfile: vi.fn(),
};

// ── Setup module mocks ──────────────────────────────
// These must be called BEFORE importing components under test:
//   vi.mock('@/context/AuthContext', ...);
//   vi.mock('@/stores/useHarvestStore', ...);
// 
// The factory functions below can be used as mock implementations:

export function createAuthContextMock(overrides: Partial<typeof mockAuthContext> = {}) {
    return {
        useAuth: () => ({ ...mockAuthContext, ...overrides }),
        AuthProvider: ({ children }: { children: React.ReactNode }) => <>{children}</>,
    };
}

export function createHarvestStoreMock(overrides: Partial<typeof mockHarvestState> = {}) {
    const state = { ...mockHarvestState, ...overrides };
    return {
        useHarvestStore: (selector?: (s: typeof state) => unknown) =>
            selector ? selector(state) : state,
    };
}

// ── Common module mocks ─────────────────────────────
// Call these in your test file's top-level vi.mock() block:
export const commonMocks = {
    '@/services/config.service': () => ({
        getConfig: () => ({ SUPABASE_URL: 'https://test.supabase.co', SUPABASE_ANON_KEY: 'test-key' }),
    }),
    '@/services/supabase': () => ({
        supabase: {
            auth: {
                getSession: vi.fn().mockResolvedValue({ data: { session: null }, error: null }),
                onAuthStateChange: vi.fn().mockReturnValue({ data: { subscription: { unsubscribe: vi.fn() } } }),
                signInWithPassword: vi.fn(),
                signOut: vi.fn().mockResolvedValue({ error: null }),
            },
            from: vi.fn().mockReturnValue({
                select: vi.fn().mockReturnThis(),
                eq: vi.fn().mockReturnThis(),
                order: vi.fn().mockReturnThis(),
                limit: vi.fn().mockResolvedValue({ data: [], error: null }),
                single: vi.fn().mockResolvedValue({ data: null, error: null }),
            }),
            channel: vi.fn().mockReturnValue({ on: vi.fn().mockReturnThis(), subscribe: vi.fn() }),
            removeAllChannels: vi.fn(),
            removeChannel: vi.fn(),
        },
    }),
    '@/services/db': () => ({ db: {} }),
    '@/services/sync.service': () => ({ syncService: { startSync: vi.fn(), stopSync: vi.fn(), processQueue: vi.fn() } }),
    '@/services/notification.service': () => ({ notificationService: { requestPermission: vi.fn() } }),
    '@/services/offline.service': () => ({ offlineService: { getPendingBuckets: vi.fn().mockResolvedValue([]), getPendingCount: vi.fn().mockResolvedValue(0) } }),
    '@/utils/logger': () => ({ logger: { info: vi.fn(), error: vi.fn(), warn: vi.fn() } }),
    '@/hooks/useToast': () => ({ useToast: () => ({ showToast: vi.fn() }) }),
    '@/config/sentry': () => ({ setSentryUser: vi.fn(), clearSentryUser: vi.fn() }),
    '@/config/analytics': () => ({ analytics: { identify: vi.fn(), capture: vi.fn(), reset: vi.fn() } }),
    '@/components/modals/ReAuthModal': () => ({ default: () => null }),
    '@/repositories/authContext.repository': () => ({
        authContextRepository: { fetchUserProfile: vi.fn().mockResolvedValue(null), upsertUserProfile: vi.fn(), fetchOrchardId: vi.fn().mockResolvedValue(null) },
    }),
};

// ── AllProviders Wrapper ────────────────────────────
// NOTE: For this wrapper to work, the consumer must vi.mock() AuthContext and
// useHarvestStore at the module level BEFORE importing components. This wrapper
// provides any additional React context providers needed like React.StrictMode.
const AllProviders: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    return <>{children}</>;
};

// ── Custom render ───────────────────────────────────
function customRender(
    ui: ReactElement,
    options?: Omit<RenderOptions, 'wrapper'>
) {
    return render(ui, { wrapper: AllProviders, ...options });
}

// Re-export everything from testing-library
export * from '@testing-library/react';
// Override render with our custom one
export { customRender as render };
