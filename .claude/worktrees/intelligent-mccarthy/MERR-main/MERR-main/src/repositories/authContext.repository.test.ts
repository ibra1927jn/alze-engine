/**
 * AuthContext Repository Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { supabase } from '@/services/supabase';
import { authContextRepository } from './authContext.repository';

function mockChain(result: { data?: unknown; error?: unknown }) {
    const chain: Record<string, unknown> = {
        select: vi.fn(() => chain), eq: vi.fn(() => chain), in: vi.fn(() => chain),
        insert: vi.fn(() => chain), update: vi.fn(() => chain),
        limit: vi.fn(() => chain), maybeSingle: vi.fn(() => chain),
        single: vi.fn(() => chain),
        then: (onF?: (v: unknown) => unknown, onR?: (r: unknown) => unknown) =>
            Promise.resolve(result).then(onF, onR),
    };
    return chain;
}

describe('authContextRepository', () => {
    let fromSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        vi.restoreAllMocks();
        fromSpy = vi.spyOn(supabase, 'from').mockReturnValue(mockChain({ data: null, error: null }) as never);
    });

    describe('getUserProfile', () => {
        it('returns user data on success', async () => {
            const user = { id: 'u1', full_name: 'John' };
            fromSpy.mockReturnValue(mockChain({ data: user, error: null }) as never);
            const result = await authContextRepository.getUserProfile('u1');
            expect(result).toEqual({ data: user, error: null });
        });

        it('retries on retriable errors', async () => {
            // First call returns 504, second returns data
            let callCount = 0;
            fromSpy.mockImplementation(() => {
                callCount++;
                if (callCount <= 1) {
                    return mockChain({ data: null, error: { message: '504 gateway timeout' } }) as never;
                }
                return mockChain({ data: { id: 'u1' }, error: null }) as never;
            });
            const result = await authContextRepository.getUserProfile('u1');
            expect(result.data).toEqual({ id: 'u1' });
        });

        it('returns error on non-retriable failure', async () => {
            fromSpy.mockReturnValue(mockChain({ data: null, error: { message: 'permission denied' } }) as never);
            const result = await authContextRepository.getUserProfile('u1');
            expect(result.error).toBeTruthy();
        });
    });

    describe('getFirstOrchardId', () => {
        it('returns orchard id', async () => {
            fromSpy.mockReturnValue(mockChain({ data: { id: 'orch-1' }, error: null }) as never);
            const result = await authContextRepository.getFirstOrchardId();
            expect(result).toBe('orch-1');
        });

        it('returns null when no orchards', async () => {
            const result = await authContextRepository.getFirstOrchardId();
            expect(result).toBeNull();
        });
    });

    describe('assignOrchard', () => {
        it('calls update on users table', async () => {
            await authContextRepository.assignOrchard('u1', 'orch-1');
            expect(fromSpy).toHaveBeenCalledWith('users');
        });
    });

    describe('checkWhitelist', () => {
        it('returns whitelist entry', async () => {
            const entry = { id: 'reg-1', role: 'picker', orchard_id: 'o1', used_at: null };
            fromSpy.mockReturnValue(mockChain({ data: entry, error: null }) as never);
            const result = await authContextRepository.checkWhitelist('Test@Email.com');
            expect(result.data).toEqual(entry);
        });

        it('returns null when not whitelisted', async () => {
            const result = await authContextRepository.checkWhitelist('unknown@test.com');
            expect(result.data).toBeNull();
        });
    });

    describe('insertUser', () => {
        it('inserts record into users table', async () => {
            await authContextRepository.insertUser({ id: 'u1', full_name: 'Test' });
            expect(fromSpy).toHaveBeenCalledWith('users');
        });
    });

    describe('markRegistrationUsed', () => {
        it('updates allowed_registrations table', async () => {
            await authContextRepository.markRegistrationUsed('reg-1');
            expect(fromSpy).toHaveBeenCalledWith('allowed_registrations');
        });
    });
});
