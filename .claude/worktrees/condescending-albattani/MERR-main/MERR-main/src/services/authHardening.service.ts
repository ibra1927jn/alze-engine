/**
 * Auth Service - Rate Limiting & Account Lockout
 * 
 * Handles login attempts tracking and account lockout logic
 */

import { logger } from '@/utils/logger';
import { rpcRepository } from '@/repositories/rpc.repository';
import { authRepository } from '@/repositories/auth.repository';

export interface LoginAttemptResult {
    success: boolean;
    error?: string;
    lockedUntil?: Date;
    remainingAttempts?: number;
}

export interface AccountLockStatus {
    isLocked: boolean;
    lockedUntil?: Date;
    remainingMs?: number;
}

export interface LoginAttempt {
    id: string;
    email: string;
    success: boolean;
    failure_reason?: string;
    ip_address?: string;
    user_agent?: string;
    attempt_time?: string;
}

export interface AccountLock {
    id?: string;
    email: string;
    locked_at: string;
    locked_until: string;
    locked_by_system: boolean;
    reason: string;
    unlocked_at: string | null;
}

// =============================================
// CONSTANTS
// =============================================

const MAX_ATTEMPTS = 5;


// =============================================
// AUTH SERVICE
// =============================================

export const authHardeningService = {
    async checkAccountLock(email: string): Promise<AccountLockStatus> {
        try {
            const { data, error } = await rpcRepository.call<boolean>('is_account_locked', {
                check_email: email.toLowerCase().trim(),
            });

            if (error) {
                logger.error('[AuthHardening] Error checking lock status:', error);
                return { isLocked: false };
            }

            if (data === true) {
                const lockData = await authRepository.getActiveLock(email.toLowerCase().trim());

                if (lockData) {
                    const lockedUntil = new Date(lockData.locked_until);
                    const remainingMs = lockedUntil.getTime() - Date.now();

                    return {
                        isLocked: true,
                        lockedUntil,
                        remainingMs: Math.max(0, remainingMs),
                    };
                }
            }

            return { isLocked: false };
        } catch (error) {
            logger.error('[AuthHardening] Error in checkAccountLock:', error);
            return { isLocked: true, remainingMs: 60000 };
        }
    },

    async getFailedLoginCount(email: string): Promise<number> {
        try {
            const { data, error } = await rpcRepository.call<number>('get_failed_login_count', {
                check_email: email.toLowerCase().trim(),
            });

            if (error) {
                logger.error('[AuthHardening] Error getting failed count:', error);
                return 0;
            }

            return data || 0;
        } catch (error) {
            logger.error('[AuthHardening] Error in getFailedLoginCount:', error);
            return 0;
        }
    },

    async logLoginAttempt(
        email: string,
        success: boolean,
        failureReason?: string
    ): Promise<void> {
        try {
            await authRepository.logAttempt({
                email: email.toLowerCase().trim(),
                success,
                failure_reason: failureReason,
                ip_address: null,
                user_agent: navigator?.userAgent || null,
            });
        } catch (error) {
            logger.error('[AuthHardening] Failed to log attempt:', error);
        }
    },

    async loginWithProtection(
        email: string,
        password: string
    ): Promise<LoginAttemptResult> {
        const normalizedEmail = email.toLowerCase().trim();

        const lockStatus = await this.checkAccountLock(normalizedEmail);
        if (lockStatus.isLocked && lockStatus.remainingMs && lockStatus.remainingMs > 0) {
            const remainingMin = Math.ceil(lockStatus.remainingMs / 60000);

            return {
                success: false,
                error: `Account temporarily locked due to multiple failed login attempts. Try again in ${remainingMin} ${remainingMin === 1 ? 'minute' : 'minutes'}.`,
                lockedUntil: lockStatus.lockedUntil,
            };
        }

        const failedCount = await this.getFailedLoginCount(normalizedEmail);
        const remainingAttempts = Math.max(0, MAX_ATTEMPTS - failedCount);

        try {
            const { error } = await rpcRepository.call('sign_in_with_password', {
                p_email: normalizedEmail,
                p_password: password,
            });
            // Note: actual auth still happens via supabase.auth in the AuthContext
            // This loginWithProtection just tracks attempts. For the actual sign-in,
            // we need supabase.auth — but that's handled by AuthContext, not here.
            // The error simulation below is for when this service is used standalone.
            // In practice, AuthContext.signIn() is called first, then this tracks attempts.
            // TODO: Decouple auth from this tracking service

            if (error) {
                await this.logLoginAttempt(normalizedEmail, false, error.message);

                const newRemaining = Math.max(0, remainingAttempts - 1);

                let errorMessage = 'Invalid email or password.';
                if (newRemaining > 0) {
                    errorMessage += ` ${newRemaining} attempt${newRemaining === 1 ? '' : 's'} remaining.`;
                } else {
                    errorMessage = 'Too many failed attempts. Account locked for 15 minutes.';
                }

                return {
                    success: false,
                    error: errorMessage,
                    remainingAttempts: newRemaining,
                };
            }

            await this.logLoginAttempt(normalizedEmail, true);

            return { success: true };
        } catch (error: unknown) {
            const errorMessage = error instanceof Error ? error.message : 'Unknown error';
            await this.logLoginAttempt(normalizedEmail, false, errorMessage);

            return {
                success: false,
                error: errorMessage || 'Login failed',
                remainingAttempts: Math.max(0, remainingAttempts - 1),
            };
        }
    },

    async unlockAccount(email: string, reason?: string): Promise<boolean> {
        try {
            const { data, error } = await rpcRepository.call<boolean>('unlock_account', {
                target_email: email.toLowerCase().trim(),
                unlock_reason_text: reason || 'Unlocked by manager',
            });

            if (error) {
                logger.error('[AuthHardening] Error unlocking account:', error);
                throw error;
            }

            return data === true;
        } catch (error) {
            logger.error('[AuthHardening] Error in unlockAccount:', error);
            throw error;
        }
    },

    async getRecentFailedAttempts(limit: number = 50): Promise<LoginAttempt[]> {
        try {
            return await authRepository.getRecentFailed(limit);
        } catch (error) {
            logger.error('[AuthHardening] Error in getRecentFailedAttempts:', error);
            return [];
        }
    },

    async getCurrentLocks(): Promise<AccountLock[]> {
        try {
            return await authRepository.getCurrentLocks();
        } catch (error) {
            logger.error('[AuthHardening] Error in getCurrentLocks:', error);
            return [];
        }
    },
};
