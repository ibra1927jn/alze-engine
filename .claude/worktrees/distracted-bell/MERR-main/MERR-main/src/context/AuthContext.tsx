/* eslint-disable react-refresh/only-export-components */
/**
 * AuthContext - Global Authentication State Management
 * 
 * **Architecture**: React Context API
 * **Why Context?**: Authentication changes infrequently, needed globally, non-performance-critical
 * **See**: `docs/architecture/state-management.md` for decision rationale
 * 
 * @module context/AuthContext
 * @see {@link file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/docs/architecture/state-management.md}
 */
import { logger } from '@/utils/logger';
import React, { createContext, useContext, useEffect, useState, useCallback, ReactNode } from 'react';
import { User } from '@supabase/supabase-js';
import { supabase } from '../services/supabase';
import { db } from '../services/db';
import { syncService } from '../services/sync.service';
import { Role, AppUser } from '../types';
import ReAuthModal from '../components/modals/ReAuthModal';
import { notificationService } from '../services/notification.service'; // 🔧 R9-Fix7
import { setSentryUser, clearSentryUser } from '../config/sentry'; // 🔧 Sentry user tracking
import { analytics } from '../config/analytics'; // 📊 PostHog event tracking
import { authContextRepository } from '@/repositories/authContext.repository';

// Types extracted to auth.types.ts for reuse
import type { AuthState, AuthContextType } from './auth.types';

// =============================================
// CONTEXT
// =============================================
const AuthContext = createContext<AuthContextType | undefined>(undefined);

// =============================================
// PROVIDER
// =============================================
export const AuthProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
    const [state, setState] = useState<AuthState>({
        user: null,
        appUser: null,
        isAuthenticated: false,
        isLoading: true,
        isSetupComplete: false,
        currentRole: null,
        userName: '',
        userEmail: '',
        orchardId: null,
        teamId: null,
        needsReAuth: false,
    });

    const updateAuthState = useCallback((updates: Partial<AuthState>) => {
        setState(prev => ({ ...prev, ...updates }));
    }, []);

    // =============================================
    // LOAD USER DATA
    // =============================================
    const loadUserData = async (userId: string) => {
        try {
            // 1. Load user from users table (with retry for 504 timeouts)
            let userData = null;
            let userError = null;
            const result = await authContextRepository.getUserProfile(userId);
            userData = result.data;
            userError = result.error;

            if (userError || !userData) {
                const errMsg = userError?.message?.toLowerCase() || '';
                const isServerError =
                    errMsg.includes('504') || errMsg.includes('502') ||
                    errMsg.includes('timeout') || errMsg.includes('gateway') ||
                    errMsg.includes('fetch') || errMsg.includes('network');

                if (isServerError) {
                    logger.error('[AuthContext] Supabase server unreachable after retries:', userError);
                    updateAuthState({ isLoading: false, isAuthenticated: false });
                    throw new Error('Servidor no disponible. Comprueba tu conexión e inténtalo de nuevo.');
                }

                logger.error('[AuthContext] User profile not found in DB:', userError);
                updateAuthState({ isLoading: false, isAuthenticated: false });
                throw new Error('User profile not found. Please contact support.');
            }

            let orchardId = userData?.orchard_id;

            // Get first available orchard if none assigned
            if (!orchardId) {
                orchardId = await authContextRepository.getFirstOrchardId();

                if (orchardId) {
                    // Persist to DB so RLS (SECURITY DEFINER) can see it!
                    await authContextRepository.assignOrchard(userId, orchardId);
                }
            }

            // 🔍 ROLE DETERMINATION
            let roleEnum: Role | null = null;
            const dbRole = userData?.role?.toLowerCase();

            const roleMap: Record<string, Role> = {
                manager: Role.MANAGER,
                team_leader: Role.TEAM_LEADER,
                bucket_runner: Role.RUNNER,
                runner: Role.RUNNER,
                qc_inspector: Role.QC_INSPECTOR,
                payroll_admin: Role.PAYROLL_ADMIN,
                admin: Role.ADMIN,
                hr_admin: Role.HR_ADMIN,
                logistics: Role.LOGISTICS,
            };
            if (dbRole) roleEnum = roleMap[dbRole] ?? null;

            // If role is unknown/null, we DO NOT default to Team Leader.
            // We set it to null, which will be caught by Routing or Login.

            if (!roleEnum) {

                logger.warn(`[AuthContext] Unknown role "${dbRole}" for user ${userId}. Access Denied.`);
                updateAuthState({ isLoading: false, isAuthenticated: false });
                throw new Error('Access Denied: You do not have a valid role assigned. Contact Manager.');
            }

            updateAuthState({
                user: { id: userId } as User,
                appUser: userData as AppUser,
                currentRole: roleEnum,
                userName: userData?.full_name || '',
                userEmail: userData?.email || '',
                isAuthenticated: true,
                isSetupComplete: true,
                isLoading: false,
                orchardId,
                teamId: userData?.team_id || null,
            });

            // 🔧 Sentry: Set user context for error tracking in production
            setSentryUser({ id: userId, email: userData?.email, role: roleEnum ?? undefined });
            // 📊 PostHog: Identify user for analytics segmentation
            analytics.identify(userId, { role: roleEnum, orchard_id: orchardId, email: userData?.email });

            return { userData, orchardId };
        } catch (error) {

            logger.error('[AuthContext] Critical Error loading user data:', error);
            // CRITICAL FIX: On error, ensure we are NOT authenticated
            updateAuthState({
                user: null,
                appUser: null,
                isLoading: false,
                isAuthenticated: false,
                currentRole: null,
            });
            return { userData: null, orchardId: null };
        }
    };

    // =============================================
    // AUTH ACTIONS
    // =============================================
    const signIn = async (email: string, password: string) => {
        updateAuthState({ isLoading: true });
        try {
            const { data, error } = await supabase.auth.signInWithPassword({ email, password });
            if (error) throw error;
            if (data.user) {
                const { userData } = await loadUserData(data.user.id);
                // 📊 PostHog: Track login event
                analytics.trackLogin(userData?.role || 'unknown', userData?.orchard_id);
                return { user: data.user, profile: userData };
            } else {
                updateAuthState({ isLoading: false });
                return { user: null, profile: null };
            }
        } catch (error) {
            updateAuthState({ isLoading: false });
            throw error;
        }
    };

    /**
     * Register new user — validates email against HR whitelist first.
     * Role is auto-assigned from allowed_registrations table.
     * Email confirmation is handled by Supabase (built-in).
     */
    const signUp = async (email: string, password: string, fullName: string) => {
        updateAuthState({ isLoading: true });
        try {
            // 1. Check whitelist — is this email pre-authorized by HR?
            const { data: registration, error: regError } = await authContextRepository.checkWhitelist(email);

            if (regError) {
                logger.error('[Auth] Whitelist lookup failed:', regError);
                throw new Error('Error al verificar autorización. Inténtalo de nuevo.');
            }

            if (!registration) {
                throw new Error('Tu email no está autorizado. Contacta con RRHH para que te den acceso.');
            }

            if (registration.used_at) {
                throw new Error('Este email ya ha sido registrado. Usa "Iniciar Sesión" o "¿Olvidaste tu contraseña?"');
            }

            const authorizedRole = registration.role as Role;
            const authorizedOrchard = registration.orchard_id;

            // 2. Create Supabase auth user (email confirmation sent automatically)
            const { data, error } = await supabase.auth.signUp({
                email,
                password,
                options: {
                    data: {
                        full_name: fullName,
                        role: authorizedRole
                    }
                }
            });
            if (error) throw error;

            if (data.user) {
                // 3. Insert into public.users with the HR-assigned role
                await authContextRepository.insertUser({
                    id: data.user.id,
                    email: email.toLowerCase().trim(),
                    full_name: fullName,
                    role: authorizedRole,
                    orchard_id: authorizedOrchard,
                    is_active: true,
                });

                // 4. Mark the whitelist entry as used
                await authContextRepository.markRegistrationUsed(registration.id);

                logger.info(`[Auth] User registered via whitelist: ${email} as ${authorizedRole}`);
            }
        } catch (error) {
            updateAuthState({ isLoading: false });
            throw error;
        }
    };

    /** Send password reset email via Supabase */
    const resetPassword = async (email: string) => {
        const { error } = await supabase.auth.resetPasswordForEmail(email, {
            redirectTo: `${window.location.origin}/login`,
        });
        if (error) throw error;
    };

    const signOut = async () => {
        try {
            // 🔧 V27: Hard-gate — block logout if there are unsynced items
            const pendingCount = await syncService.getPendingCount();
            if (pendingCount > 0) {
                const confirmed = window.confirm(
                    `⚠️ ALERTA CRÍTICA: Tienes ${pendingCount} registros sin sincronizar.\n\n` +
                    `Si cierras sesión ahora, estos datos se perderán permanentemente ` +
                    `(incluyendo información de nómina).\n\n` +
                    `Conecta a internet y espera a que todo se sincronice antes de cerrar sesión.\n\n` +
                    `¿Estás SEGURO de que quieres cerrar sesión y PERDER estos datos?`
                );
                if (!confirmed) return; // Abort sign-out
                logger.warn(`[Auth] User forced sign-out with ${pendingCount} pending items — DATA WILL BE LOST`);
            }

            // 🔧 R9-Fix7: Stop notification timer to prevent post-logout 401 errors & timer duplication
            notificationService.stopChecking();
            // 🔧 Sentry: Clear user context on logout
            clearSentryUser();
            // 📊 PostHog: Track logout event + clear identity
            analytics.trackLogout();
            // 🔧 U6: Kill realtime channels BEFORE clearing auth
            supabase.removeAllChannels();
            await supabase.auth.signOut();
        } catch (error) {
            logger.error('Error signing out from Supabase:', error);
        } finally {
            // 🔧 U6: Wipe Dexie to prevent cross-session data leak on shared tablet
            try {
                await db.delete();
            } catch (e) {
                logger.error('[Auth] Dexie wipe failed:', e);
            }
            localStorage.clear();
            // 🔧 V26: Force hard reload to re-instantiate Dexie engine
            // Without this, the JS db instance is a zombie pointing to deleted IndexedDB
            window.location.reload();
        }
    };

    const logout = signOut;

    // Demo mode setup (DISABLED FOR PRODUCTION)
    const completeSetup = (_role: Role, _name: string, _email: string) => {

        logger.warn("Demo mode is disabled. Please use real SignUp.");
        // No-op or throw error
    };

    // =============================================
    // EFFECTS
    // =============================================
    useEffect(() => {
        // 1. Initial session check
        supabase.auth.getSession().then(({ data: { session } }) => {
            if (session?.user) {
                loadUserData(session.user.id);
            } else {
                setState(prev => ({ ...prev, isLoading: false }));
            }
        });

        // 2. Auth state change listener (handles SIGNED_IN, SIGNED_OUT, TOKEN_REFRESHED)
        const {
            data: { subscription },
        } = supabase.auth.onAuthStateChange(async (event, session) => {
            // 🔄 TOKEN_REFRESHED: Token was silently renewed — no UI action needed
            if (event === 'TOKEN_REFRESHED') {
                logger.info('[Auth] Token refreshed silently in background');
                return; // Don't trigger loadUserData or signOut
            }

            if (session?.user && !state.isAuthenticated) {
                loadUserData(session.user.id);
            } else if (!session && state.isAuthenticated) {
                // 🔧 R8-Fix2: Don't call signOut() if pending data exists —
                // that triggers V27 guard + Dexie wipe deadlock.
                const pendingCount = await syncService.getPendingCount();
                if (pendingCount > 0) {
                    // 🔧 R9-Fix11: Try refreshSession() first
                    const { data: refreshData } = await supabase.auth.refreshSession();
                    if (refreshData?.session) {
                        logger.info(`[Auth] Session refreshed silently — ${pendingCount} pending items safe`);
                        loadUserData(refreshData.session.user.id);
                    } else {
                        logger.warn(`[Auth] Session expired, refresh failed, ${pendingCount} pending items — showing re-auth modal`);
                        setState(prev => ({ ...prev, needsReAuth: true }));
                    }
                } else {
                    signOut();
                }
            }
        });

        // 3. ⏱️ Proactive refresh timer — every 50 min (token expires at 60)
        //    Fails silently if offline (no harm — cached token works for local ops)
        const refreshTimer = setInterval(async () => {
            if (navigator.onLine) {
                logger.info('[Auth] Proactive token refresh (50-min timer)...');
                await supabase.auth.refreshSession().catch(() => {
                    logger.warn('[Auth] Proactive refresh failed — will retry next interval');
                });
            }
        }, 50 * 60 * 1000);

        // 4. 📱 Visibility-based refresh with 3-min throttle
        //    Prevents DDoS (HTTP 429) when user tab-switches rapidly
        let lastVisibilityRefresh = 0;
        const handleVisibilityChange = async () => {
            if (document.visibilityState === 'visible' && navigator.onLine) {
                const now = Date.now();
                if (now - lastVisibilityRefresh > 180_000) { // 3-min cooldown
                    lastVisibilityRefresh = now;
                    logger.info('[Auth] App in foreground — ensuring token health...');
                    await supabase.auth.refreshSession().catch(() => {
                        logger.warn('[Auth] Visibility refresh failed');
                    });
                }
            }
        };
        document.addEventListener('visibilitychange', handleVisibilityChange);

        return () => {
            subscription.unsubscribe();
            clearInterval(refreshTimer);
            document.removeEventListener('visibilitychange', handleVisibilityChange);
        };
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []); // Mount-only: auth subscription must not re-subscribe on state changes

    // =============================================
    // CONTEXT VALUE
    // =============================================
    const contextValue: AuthContextType = {
        ...state,
        signIn,
        signUp,
        resetPassword,
        signOut,
        logout,
        completeSetup,
        updateAuthState,
    };

    return (
        <AuthContext.Provider value={contextValue}>
            {children}
            {/* 🔧 R8-Fix2: Ineludible re-auth modal when JWT expires with pending data */}
            {state.needsReAuth && state.userEmail && (
                <ReAuthModal
                    email={state.userEmail}
                    onReAuthenticated={() => {
                        setState(prev => ({ ...prev, needsReAuth: false }));
                        // Resume sync after re-authentication
                        syncService.processQueue();
                    }}
                />
            )}
        </AuthContext.Provider>
    );
};

// =============================================
// HOOK
// =============================================
export const useAuth = (): AuthContextType => {
    const context = useContext(AuthContext);
    if (!context) {
        throw new Error('useAuth must be used within an AuthProvider');
    }
    return context;
};

export { supabase };
export default AuthContext;
