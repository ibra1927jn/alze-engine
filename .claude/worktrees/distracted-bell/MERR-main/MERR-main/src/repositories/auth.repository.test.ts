/**
 * Auth Repository Tests — vi.spyOn approach
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { supabase } from '@/services/supabase';
import { authRepository } from './auth.repository';

function mockChain(result: { data?: unknown; error?: unknown }) {
    const chain: Record<string, unknown> = {
        select: vi.fn(() => chain), eq: vi.fn(() => chain), insert: vi.fn(() => chain),
        order: vi.fn(() => chain), limit: vi.fn(() => chain), single: vi.fn(() => chain),
        gt: vi.fn(() => chain), is: vi.fn(() => chain),
        then: (onF?: (v: unknown) => unknown, onR?: (r: unknown) => unknown) =>
            Promise.resolve(result).then(onF, onR),
    };
    return chain;
}

describe('authRepository', () => {
    let fromSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        vi.restoreAllMocks();
        fromSpy = vi.spyOn(supabase, 'from').mockReturnValue(mockChain({ data: null, error: null }) as never);
    });

    it('logAttempt inserts entry', async () => {
        await expect(authRepository.logAttempt({ email: 'user@test.com' })).resolves.toBeUndefined();
    });

    it('getRecentFailed returns failed attempts', async () => {
        fromSpy.mockReturnValue(mockChain({ data: [{ email: 'user@test.com', success: false }], error: null }) as never);
        const result = await authRepository.getRecentFailed();
        expect(result).toHaveLength(1);
    });

    it('getRecentFailed throws on error', async () => {
        fromSpy.mockReturnValue(mockChain({ data: null, error: { message: 'err' } }) as never);
        await expect(authRepository.getRecentFailed()).rejects.toBeTruthy();
    });

    it('getActiveLock returns lock when active', async () => {
        fromSpy.mockReturnValue(mockChain({ data: { locked_until: '2026-03-01T14:00:00' }, error: null }) as never);
        const result = await authRepository.getActiveLock('user@test.com');
        expect(result).toEqual({ locked_until: '2026-03-01T14:00:00' });
    });

    it('getActiveLock returns null when no lock', async () => {
        const result = await authRepository.getActiveLock('user@test.com');
        expect(result).toBeNull();
    });

    it('getCurrentLocks returns locks', async () => {
        fromSpy.mockReturnValue(mockChain({ data: [{ email: 'user@test.com' }], error: null }) as never);
        const result = await authRepository.getCurrentLocks();
        expect(result).toHaveLength(1);
    });

    it('getCurrentLocks throws on error', async () => {
        fromSpy.mockReturnValue(mockChain({ data: null, error: { message: 'err' } }) as never);
        await expect(authRepository.getCurrentLocks()).rejects.toBeTruthy();
    });
});
