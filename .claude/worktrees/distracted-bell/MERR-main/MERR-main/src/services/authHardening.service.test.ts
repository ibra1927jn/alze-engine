// =============================================
// AUTH HARDENING SERVICE TESTS
// =============================================
import { describe, it, expect, beforeEach, vi } from 'vitest';

// Mock rpcRepository
const mockRpcCall = vi.fn();
vi.mock('@/repositories/rpc.repository', () => ({
    rpcRepository: {
        call: (...args: unknown[]) => mockRpcCall(...args),
    },
}));

// Mock authRepository
const mockGetActiveLock = vi.fn();
const mockLogAttempt = vi.fn();
const mockGetRecentFailed = vi.fn();
const mockGetCurrentLocks = vi.fn();
vi.mock('@/repositories/auth.repository', () => ({
    authRepository: {
        getActiveLock: (...args: unknown[]) => mockGetActiveLock(...args),
        logAttempt: (...args: unknown[]) => mockLogAttempt(...args),
        getRecentFailed: (...args: unknown[]) => mockGetRecentFailed(...args),
        getCurrentLocks: () => mockGetCurrentLocks(),
    },
}));

vi.mock('@/utils/nzst', () => ({
    nowNZST: () => '2026-02-13T10:00:00+13:00',
}));

vi.mock('@/utils/logger', () => ({
    logger: {
        info: vi.fn(),
        warn: vi.fn(),
        error: vi.fn(),
    },
}));

import { authHardeningService } from './authHardening.service';

// =============================================
// TESTS
// =============================================

describe('Auth Hardening Service', () => {
    beforeEach(() => {
        vi.clearAllMocks();
    });

    // =============================================
    // checkAccountLock
    // =============================================
    describe('checkAccountLock', () => {
        it('should return not locked when RPC returns false', async () => {
            mockRpcCall.mockResolvedValue({ data: false, error: null });

            const result = await authHardeningService.checkAccountLock('test@example.com');

            expect(result.isLocked).toBe(false);
            expect(mockRpcCall).toHaveBeenCalledWith('is_account_locked', {
                check_email: 'test@example.com',
            });
        });

        it('should return locked with details when RPC returns true', async () => {
            const lockedUntil = new Date(Date.now() + 900000).toISOString(); // 15 min from now
            mockRpcCall.mockResolvedValue({ data: true, error: null });
            mockGetActiveLock.mockResolvedValue({ locked_until: lockedUntil });

            const result = await authHardeningService.checkAccountLock('test@example.com');

            expect(result.isLocked).toBe(true);
            expect(result.lockedUntil).toBeInstanceOf(Date);
            expect(result.remainingMs).toBeGreaterThan(0);
        });

        it('should normalize email to lowercase and trim', async () => {
            mockRpcCall.mockResolvedValue({ data: false, error: null });

            await authHardeningService.checkAccountLock('  Test@EXAMPLE.com  ');

            expect(mockRpcCall).toHaveBeenCalledWith('is_account_locked', {
                check_email: 'test@example.com',
            });
        });

        it('should fail open on RPC error (not block users)', async () => {
            mockRpcCall.mockResolvedValue({ data: null, error: { message: 'DB error' } });

            const result = await authHardeningService.checkAccountLock('test@example.com');

            expect(result.isLocked).toBe(false);
        });

        it('should fail closed on exception (V11 brute-force prevention)', async () => {
            mockRpcCall.mockRejectedValue(new Error('Network timeout'));

            const result = await authHardeningService.checkAccountLock('test@example.com');

            // V11 fix: fail closed to prevent brute-force bypass during connectivity issues
            expect(result.isLocked).toBe(true);
        });
    });

    // =============================================
    // getFailedLoginCount
    // =============================================
    describe('getFailedLoginCount', () => {
        it('should return count from RPC', async () => {
            mockRpcCall.mockResolvedValue({ data: 3, error: null });

            const count = await authHardeningService.getFailedLoginCount('test@example.com');

            expect(count).toBe(3);
        });

        it('should return 0 on error', async () => {
            mockRpcCall.mockResolvedValue({ data: null, error: { message: 'DB error' } });

            const count = await authHardeningService.getFailedLoginCount('test@example.com');

            expect(count).toBe(0);
        });

        it('should return 0 on exception', async () => {
            mockRpcCall.mockRejectedValue(new Error('Network timeout'));

            const count = await authHardeningService.getFailedLoginCount('test@example.com');

            expect(count).toBe(0);
        });

        it('should normalize email', async () => {
            mockRpcCall.mockResolvedValue({ data: 0, error: null });

            await authHardeningService.getFailedLoginCount('  Admin@EXAMPLE.COM ');

            expect(mockRpcCall).toHaveBeenCalledWith('get_failed_login_count', {
                check_email: 'admin@example.com',
            });
        });
    });

    // =============================================
    // logLoginAttempt
    // =============================================
    describe('logLoginAttempt', () => {
        it('should insert login attempt record', async () => {
            mockLogAttempt.mockResolvedValue(undefined);

            await authHardeningService.logLoginAttempt('test@example.com', true);

            expect(mockLogAttempt).toHaveBeenCalledWith(expect.objectContaining({
                email: 'test@example.com',
                success: true,
                failure_reason: undefined,
            }));
        });

        it('should include failure reason on failed attempt', async () => {
            mockLogAttempt.mockResolvedValue(undefined);

            await authHardeningService.logLoginAttempt('test@example.com', false, 'Invalid credentials');

            expect(mockLogAttempt).toHaveBeenCalledWith(expect.objectContaining({
                success: false,
                failure_reason: 'Invalid credentials',
            }));
        });

        it('should not throw on insert error (logging should not break login)', async () => {
            mockLogAttempt.mockRejectedValue(new Error('DB error'));

            // Should NOT throw
            await expect(
                authHardeningService.logLoginAttempt('test@example.com', true)
            ).resolves.toBeUndefined();
        });
    });

    // =============================================
    // loginWithProtection
    // =============================================
    describe('loginWithProtection', () => {
        beforeEach(() => {
            // By default: not locked, 0 failed attempts
            mockRpcCall.mockImplementation((funcName: string) => {
                if (funcName === 'is_account_locked') {
                    return Promise.resolve({ data: false, error: null });
                }
                if (funcName === 'get_failed_login_count') {
                    return Promise.resolve({ data: 0, error: null });
                }
                // sign_in_with_password — success by default
                if (funcName === 'sign_in_with_password') {
                    return Promise.resolve({ data: null, error: null });
                }
                return Promise.resolve({ data: null, error: null });
            });

            // Default: log attempts succeeds
            mockLogAttempt.mockResolvedValue(undefined);
        });

        it('should reject login when account is locked', async () => {
            mockRpcCall.mockImplementation((funcName: string) => {
                if (funcName === 'is_account_locked') {
                    return Promise.resolve({ data: true, error: null });
                }
                return Promise.resolve({ data: 0, error: null });
            });
            const lockedUntil = new Date(Date.now() + 900000);
            mockGetActiveLock.mockResolvedValue({ locked_until: lockedUntil.toISOString() });

            const result = await authHardeningService.loginWithProtection('test@example.com', 'pass123');

            expect(result.success).toBe(false);
            expect(result.error).toContain('temporarily locked');
            expect(result.lockedUntil).toBeInstanceOf(Date);
        });

        it('should return success on valid credentials', async () => {
            // sign_in_with_password returns no error
            const result = await authHardeningService.loginWithProtection('test@example.com', 'correct-pass');

            expect(result.success).toBe(true);
            expect(result.error).toBeUndefined();
        });

        it('should return remaining attempts on failed login', async () => {
            mockRpcCall.mockImplementation((funcName: string) => {
                if (funcName === 'is_account_locked') {
                    return Promise.resolve({ data: false, error: null });
                }
                if (funcName === 'get_failed_login_count') {
                    return Promise.resolve({ data: 2, error: null }); // 2 prior failures
                }
                // sign_in_with_password fails
                if (funcName === 'sign_in_with_password') {
                    return Promise.resolve({ data: null, error: { message: 'Invalid login credentials' } });
                }
                return Promise.resolve({ data: null, error: null });
            });

            const result = await authHardeningService.loginWithProtection('test@example.com', 'wrong-pass');

            expect(result.success).toBe(false);
            expect(result.remainingAttempts).toBe(2); // 5 max - 2 prior - 1 this = 2
            expect(result.error).toContain('2 attempts remaining');
        });

        it('should show lockout message when last attempt exhausted', async () => {
            mockRpcCall.mockImplementation((funcName: string) => {
                if (funcName === 'is_account_locked') {
                    return Promise.resolve({ data: false, error: null });
                }
                if (funcName === 'get_failed_login_count') {
                    return Promise.resolve({ data: 4, error: null }); // 4 prior = 1 remaining
                }
                if (funcName === 'sign_in_with_password') {
                    return Promise.resolve({ data: null, error: { message: 'Invalid login credentials' } });
                }
                return Promise.resolve({ data: null, error: null });
            });

            const result = await authHardeningService.loginWithProtection('test@example.com', 'wrong-pass');

            expect(result.success).toBe(false);
            expect(result.remainingAttempts).toBe(0);
            expect(result.error).toContain('locked for 15 minutes');
        });

        it('should normalize email before all operations', async () => {
            await authHardeningService.loginWithProtection('  Admin@TEST.com  ', 'pass');

            expect(mockRpcCall).toHaveBeenCalledWith('is_account_locked', {
                check_email: 'admin@test.com',
            });
        });

        it('should handle unexpected exceptions gracefully', async () => {
            mockRpcCall.mockImplementation((funcName: string) => {
                if (funcName === 'is_account_locked') {
                    return Promise.resolve({ data: false, error: null });
                }
                if (funcName === 'get_failed_login_count') {
                    return Promise.resolve({ data: 0, error: null });
                }
                if (funcName === 'sign_in_with_password') {
                    return Promise.reject(new Error('Network failure'));
                }
                return Promise.resolve({ data: null, error: null });
            });

            const result = await authHardeningService.loginWithProtection('test@example.com', 'pass');

            expect(result.success).toBe(false);
            expect(result.error).toContain('Network failure');
        });
    });

    // =============================================
    // unlockAccount
    // =============================================
    describe('unlockAccount', () => {
        it('should call RPC to unlock account', async () => {
            mockRpcCall.mockResolvedValue({ data: true, error: null });

            const result = await authHardeningService.unlockAccount('locked@test.com', 'Manager override');

            expect(mockRpcCall).toHaveBeenCalledWith('unlock_account', {
                target_email: 'locked@test.com',
                unlock_reason_text: 'Manager override',
            });
            expect(result).toBe(true);
        });

        it('should use default reason when none provided', async () => {
            mockRpcCall.mockResolvedValue({ data: true, error: null });

            await authHardeningService.unlockAccount('locked@test.com');

            expect(mockRpcCall).toHaveBeenCalledWith('unlock_account', {
                target_email: 'locked@test.com',
                unlock_reason_text: 'Unlocked by manager',
            });
        });

        it('should throw on RPC error', async () => {
            mockRpcCall.mockResolvedValue({ data: null, error: { message: 'Permission denied' } });

            await expect(
                authHardeningService.unlockAccount('locked@test.com')
            ).rejects.toBeDefined();
        });
    });

    // =============================================
    // getRecentFailedAttempts
    // =============================================
    describe('getRecentFailedAttempts', () => {
        it('should return failed attempts from database', async () => {
            const mockData = [
                { id: '1', email: 'test@test.com', success: false, attempt_time: '2026-02-13T10:00:00' },
            ];
            mockGetRecentFailed.mockResolvedValue(mockData);

            const attempts = await authHardeningService.getRecentFailedAttempts(10);

            expect(attempts).toHaveLength(1);
        });

        it('should return empty array on error', async () => {
            mockGetRecentFailed.mockRejectedValue(new Error('DB error'));

            const attempts = await authHardeningService.getRecentFailedAttempts();

            expect(attempts).toEqual([]);
        });
    });

    // =============================================
    // getCurrentLocks
    // =============================================
    describe('getCurrentLocks', () => {
        it('should return active locks', async () => {
            const mockLocks = [
                { id: '1', email: 'locked@test.com', locked_at: '2026-02-13T09:45:00', locked_until: '2026-02-13T10:00:00' },
            ];
            mockGetCurrentLocks.mockResolvedValue(mockLocks);

            const locks = await authHardeningService.getCurrentLocks();

            expect(locks).toHaveLength(1);
        });

        it('should return empty array on error', async () => {
            mockGetCurrentLocks.mockRejectedValue(new Error('DB error'));

            const locks = await authHardeningService.getCurrentLocks();

            expect(locks).toEqual([]);
        });
    });
});
