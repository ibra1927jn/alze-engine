/**
 * Tests for AuthContext — provider, hook, and auth actions
 * 
 * Tests: signIn, signUp, signOut, resetPassword, loadUserData, error flows
 * @module context/AuthContext.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, renderHook, act, waitFor } from '@testing-library/react';

// ── Hoisted mocks (vitest requirement) ────────────────
const mocks = vi.hoisted(() => ({
    getSession: vi.fn().mockResolvedValue({ data: { session: null }, error: null }),
    onAuthStateChange: vi.fn().mockReturnValue({ data: { subscription: { unsubscribe: vi.fn() } } }),
    signInWithPassword: vi.fn(),
    signUpAuth: vi.fn(),
    signOutAuth: vi.fn().mockResolvedValue({ error: null }),
    resetPasswordForEmail: vi.fn().mockResolvedValue({ error: null }),
    refreshSession: vi.fn().mockResolvedValue({ data: { session: null }, error: null }),
    getUserProfile: vi.fn(),
    getFirstOrchardId: vi.fn(),
    assignOrchard: vi.fn(),
    checkWhitelist: vi.fn(),
    insertUser: vi.fn(),
    markRegistrationUsed: vi.fn(),
    getPendingCount: vi.fn().mockResolvedValue(0),
    setSentryUser: vi.fn(),
    clearSentryUser: vi.fn(),
    analytics: { identify: vi.fn(), trackLogin: vi.fn(), trackLogout: vi.fn(), capture: vi.fn(), reset: vi.fn() },
}));

vi.mock('@/services/config.service', () => ({
    getConfig: () => ({ SUPABASE_URL: 'https://x.supabase.co', SUPABASE_ANON_KEY: 'k' }),
}));
vi.mock('@/services/supabase', () => ({
    supabase: {
        auth: {
            getSession: mocks.getSession,
            onAuthStateChange: mocks.onAuthStateChange,
            signInWithPassword: mocks.signInWithPassword,
            signUp: mocks.signUpAuth,
            signOut: mocks.signOutAuth,
            resetPasswordForEmail: mocks.resetPasswordForEmail,
            refreshSession: mocks.refreshSession,
        },
        from: vi.fn().mockReturnValue({
            select: vi.fn().mockReturnThis(), eq: vi.fn().mockReturnThis(),
            single: vi.fn().mockResolvedValue({ data: null, error: null }),
            insert: vi.fn().mockResolvedValue({ data: null, error: null }),
            update: vi.fn().mockReturnThis(),
        }),
        channel: vi.fn().mockReturnValue({ on: vi.fn().mockReturnThis(), subscribe: vi.fn() }),
        removeChannel: vi.fn(), removeAllChannels: vi.fn(),
    },
}));
vi.mock('@/repositories/authContext.repository', () => ({
    authContextRepository: {
        getUserProfile: mocks.getUserProfile,
        getFirstOrchardId: mocks.getFirstOrchardId,
        assignOrchard: mocks.assignOrchard,
        checkWhitelist: mocks.checkWhitelist,
        insertUser: mocks.insertUser,
        markRegistrationUsed: mocks.markRegistrationUsed,
        fetchUserProfile: vi.fn().mockResolvedValue(null),
        upsertUserProfile: vi.fn().mockResolvedValue(null),
        fetchOrchardId: vi.fn().mockResolvedValue(null),
    },
}));
vi.mock('@/services/db', () => ({
    db: { delete: vi.fn().mockResolvedValue(undefined), user_cache: { get: vi.fn(), put: vi.fn() }, settings_cache: { get: vi.fn(), put: vi.fn() } },
}));
vi.mock('@/services/sync.service', () => ({
    syncService: { startSync: vi.fn(), stopSync: vi.fn(), processQueue: vi.fn(), getPendingCount: mocks.getPendingCount },
}));
vi.mock('@/services/notification.service', () => ({
    notificationService: { requestPermission: vi.fn(), stopChecking: vi.fn() },
}));
vi.mock('@/components/modals/ReAuthModal', () => ({ default: () => null }));
vi.mock('@/utils/logger', () => ({ logger: { info: vi.fn(), error: vi.fn(), warn: vi.fn(), debug: vi.fn() } }));
vi.mock('@/hooks/useToast', () => ({ useToast: () => ({ showToast: vi.fn() }) }));
vi.mock('@/config/sentry', () => ({ setSentryUser: mocks.setSentryUser, clearSentryUser: mocks.clearSentryUser }));
vi.mock('@/config/analytics', () => ({ analytics: mocks.analytics }));

import { AuthProvider, useAuth } from './AuthContext';

// ── Helpers ──────────────────────────────────────────
const wrapper = ({ children }: { children: React.ReactNode }) => <AuthProvider>{children}</AuthProvider>;

const mockManagerProfile = {
    id: 'user-123', email: 'manager@test.co.nz', full_name: 'Test Manager',
    role: 'manager', orchard_id: 'orchard-1', team_id: null, is_active: true,
};

describe('AuthContext', () => {
    beforeEach(() => {
        vi.clearAllMocks();
        mocks.getSession.mockResolvedValue({ data: { session: null }, error: null });
        mocks.getPendingCount.mockResolvedValue(0);
    });

    // ── Provider & Hook ──────────────────────────────
    describe('Provider & Hook', () => {
        it('renders children correctly', () => {
            render(<AuthProvider><p>Child</p></AuthProvider>);
            expect(screen.getByText('Child')).toBeTruthy();
        });

        it('useAuth returns complete AuthContextType shape', () => {
            const { result } = renderHook(() => useAuth(), { wrapper });
            expect(result.current).toHaveProperty('user');
            expect(result.current).toHaveProperty('appUser');
            expect(result.current).toHaveProperty('isAuthenticated');
            expect(result.current).toHaveProperty('isLoading');
            expect(result.current).toHaveProperty('isSetupComplete');
            expect(result.current).toHaveProperty('currentRole');
            expect(result.current).toHaveProperty('needsReAuth');
            expect(typeof result.current.signIn).toBe('function');
            expect(typeof result.current.signUp).toBe('function');
            expect(typeof result.current.signOut).toBe('function');
            expect(typeof result.current.logout).toBe('function');
            expect(typeof result.current.resetPassword).toBe('function');
            expect(typeof result.current.completeSetup).toBe('function');
            expect(typeof result.current.updateAuthState).toBe('function');
        });

        it('useAuth throws if used outside AuthProvider', () => {
            expect(() => renderHook(() => useAuth())).toThrow('useAuth must be used within an AuthProvider');
        });
    });

    // ── Initial State ────────────────────────────────
    describe('Initial State', () => {
        it('user is initially null', () => {
            const { result } = renderHook(() => useAuth(), { wrapper });
            expect(result.current.user).toBeNull();
        });

        it('isLoading is initially true', () => {
            const { result } = renderHook(() => useAuth(), { wrapper });
            expect(result.current.isLoading).toBe(true);
        });

        it('isAuthenticated is initially false', () => {
            const { result } = renderHook(() => useAuth(), { wrapper });
            expect(result.current.isAuthenticated).toBe(false);
        });

        it('currentRole is initially null', () => {
            const { result } = renderHook(() => useAuth(), { wrapper });
            expect(result.current.currentRole).toBeNull();
        });

        it('needsReAuth is initially false', () => {
            const { result } = renderHook(() => useAuth(), { wrapper });
            expect(result.current.needsReAuth).toBe(false);
        });
    });

    // ── signIn ───────────────────────────────────────
    describe('signIn', () => {
        it('calls supabase signInWithPassword', async () => {
            mocks.signInWithPassword.mockResolvedValue({
                data: { user: { id: 'user-123' }, session: {} },
                error: null,
            });
            mocks.getUserProfile.mockResolvedValue({ data: mockManagerProfile, error: null });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await act(async () => {
                await result.current.signIn('manager@test.co.nz', 'pass123');
            });

            expect(mocks.signInWithPassword).toHaveBeenCalledWith({
                email: 'manager@test.co.nz', password: 'pass123',
            });
        });

        it('loads user data and sets Sentry/analytics on success', async () => {
            mocks.signInWithPassword.mockResolvedValue({
                data: { user: { id: 'user-123' }, session: {} }, error: null,
            });
            mocks.getUserProfile.mockResolvedValue({ data: mockManagerProfile, error: null });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await act(async () => {
                await result.current.signIn('manager@test.co.nz', 'pass123');
            });

            expect(mocks.setSentryUser).toHaveBeenCalled();
            expect(mocks.analytics.identify).toHaveBeenCalled();
            expect(mocks.analytics.trackLogin).toHaveBeenCalled();
        });

        it('throws error when supabase auth fails', async () => {
            mocks.signInWithPassword.mockResolvedValue({
                data: { user: null }, error: { message: 'Invalid credentials' },
            });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await expect(
                act(async () => { await result.current.signIn('bad@email.com', 'wrong'); })
            ).rejects.toThrow();
        });

        it('returns null profile when no user data', async () => {
            mocks.signInWithPassword.mockResolvedValue({
                data: { user: null, session: null }, error: null,
            });

            const { result } = renderHook(() => useAuth(), { wrapper });

            let response;
            await act(async () => {
                response = await result.current.signIn('test@test.com', 'pass');
            });

            expect(response).toEqual({ user: null, profile: null });
        });
    });

    // ── signUp ───────────────────────────────────────
    describe('signUp', () => {
        it('checks whitelist then creates user', async () => {
            mocks.checkWhitelist.mockResolvedValue({
                data: { id: 'reg-1', role: 'manager', orchard_id: 'orchard-1', used_at: null },
                error: null,
            });
            mocks.signUpAuth.mockResolvedValue({
                data: { user: { id: 'new-user-1' }, session: {} }, error: null,
            });
            mocks.insertUser.mockResolvedValue({ error: null });
            mocks.markRegistrationUsed.mockResolvedValue({ error: null });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await act(async () => {
                await result.current.signUp('new@test.com', 'pass123', 'New User');
            });

            expect(mocks.checkWhitelist).toHaveBeenCalledWith('new@test.com');
            expect(mocks.insertUser).toHaveBeenCalled();
            expect(mocks.markRegistrationUsed).toHaveBeenCalledWith('reg-1');
        });

        it('throws when email not on whitelist', async () => {
            mocks.checkWhitelist.mockResolvedValue({ data: null, error: null });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await expect(
                act(async () => { await result.current.signUp('nobody@test.com', 'p', 'Nobody'); })
            ).rejects.toThrow('no está autorizado');
        });

        it('throws when whitelist entry already used', async () => {
            mocks.checkWhitelist.mockResolvedValue({
                data: { id: 'reg-1', role: 'manager', used_at: '2024-01-01' }, error: null,
            });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await expect(
                act(async () => { await result.current.signUp('used@test.com', 'p', 'X'); })
            ).rejects.toThrow('ya ha sido registrado');
        });

        it('throws when whitelist lookup fails', async () => {
            mocks.checkWhitelist.mockResolvedValue({ data: null, error: { message: 'DB error' } });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await expect(
                act(async () => { await result.current.signUp('x@t.com', 'p', 'X'); })
            ).rejects.toThrow('verificar autorización');
        });
    });

    // ── resetPassword ────────────────────────────────
    describe('resetPassword', () => {
        it('calls supabase resetPasswordForEmail', async () => {
            const { result } = renderHook(() => useAuth(), { wrapper });

            await act(async () => { await result.current.resetPassword('test@test.com'); });

            expect(mocks.resetPasswordForEmail).toHaveBeenCalledWith(
                'test@test.com', { redirectTo: expect.stringContaining('/login') }
            );
        });

        it('throws when reset fails', async () => {
            mocks.resetPasswordForEmail.mockResolvedValue({ error: { message: 'Not found' } });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await expect(
                act(async () => { await result.current.resetPassword('bad@test.com'); })
            ).rejects.toThrow();
        });
    });

    // ── signOut ──────────────────────────────────────
    describe('signOut', () => {
        it('calls supabase signOut + clears sentry', async () => {
            mocks.getPendingCount.mockResolvedValue(0);

            const { result } = renderHook(() => useAuth(), { wrapper });

            await act(async () => { result.current.updateAuthState({ isLoading: false }); });
            await act(async () => { await result.current.signOut(); });

            expect(mocks.signOutAuth).toHaveBeenCalled();
            expect(mocks.clearSentryUser).toHaveBeenCalled();
        });
    });

    // ── completeSetup ────────────────────────────────
    describe('completeSetup', () => {
        it('is a no-op in production', () => {
            const { result } = renderHook(() => useAuth(), { wrapper });
            expect(() => result.current.completeSetup('manager' as any, 'T', 't@t.com')).not.toThrow();
        });
    });

    // ── updateAuthState ──────────────────────────────
    describe('updateAuthState', () => {
        it('updates partial state', async () => {
            const { result } = renderHook(() => useAuth(), { wrapper });

            await act(async () => { result.current.updateAuthState({ userName: 'Updated' }); });

            expect(result.current.userName).toBe('Updated');
        });

        it('preserves existing state', async () => {
            const { result } = renderHook(() => useAuth(), { wrapper });

            await act(async () => { result.current.updateAuthState({ userName: 'Updated' }); });

            expect(result.current.userEmail).toBe('');
            expect(result.current.isAuthenticated).toBe(false);
        });
    });

    // ── Session Init ─────────────────────────────────
    describe('Session Initialization', () => {
        it('checks for existing session on mount', () => {
            renderHook(() => useAuth(), { wrapper });
            expect(mocks.getSession).toHaveBeenCalled();
        });

        it('sets up auth state change listener', () => {
            renderHook(() => useAuth(), { wrapper });
            expect(mocks.onAuthStateChange).toHaveBeenCalled();
        });

        it('loads user data when session exists', async () => {
            mocks.getSession.mockResolvedValue({
                data: { session: { user: { id: 'user-123' } } }, error: null,
            });
            mocks.getUserProfile.mockResolvedValue({ data: mockManagerProfile, error: null });

            const { result } = renderHook(() => useAuth(), { wrapper });

            await waitFor(() => { expect(result.current.isAuthenticated).toBe(true); });
            expect(result.current.currentRole).toBe('manager');
            expect(result.current.userName).toBe('Test Manager');
        });

        it('sets isLoading false when no session', async () => {
            const { result } = renderHook(() => useAuth(), { wrapper });

            await waitFor(() => { expect(result.current.isLoading).toBe(false); });
        });
    });
});
