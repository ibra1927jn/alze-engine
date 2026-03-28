/**
 * user.service.test.ts — Unit tests
 *
 * userService uses raw Supabase chains (no repository layer).
 * Methods: getUserProfile, getOrchardUsers, getAvailableUsers,
 * getAvailableTeamLeaders, getAvailableRunners, assignUserToOrchard, unassignUserFromOrchard
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn() },
}));

vi.mock('@/utils/nzst', () => ({
    nowNZST: vi.fn(() => '2026-03-04T12:00:00+13:00'),
    todayNZST: vi.fn(() => '2026-03-04'),
}));

import { supabase } from './supabase';
import { userService } from './user.service';

// Helper: creates a chainable supabase mock for simple queries
function mockChain(data: unknown, error: unknown = null) {
    const chain: Record<string, unknown> = {};
    chain.select = vi.fn().mockReturnValue(chain);
    chain.eq = vi.fn().mockReturnValue(chain);
    chain.order = vi.fn().mockResolvedValue({ data, error });
    chain.single = vi.fn().mockResolvedValue({ data, error });
    chain.maybeSingle = vi.fn().mockResolvedValue({ data, error });
    chain.update = vi.fn().mockReturnValue(chain);
    chain.insert = vi.fn().mockReturnValue(chain);
    chain.delete = vi.fn().mockReturnValue(chain);
    chain.gte = vi.fn().mockReturnValue(chain);
    chain.limit = vi.fn().mockResolvedValue({ data, error });
    return chain;
}

describe('userService', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
    });

    describe('getUserProfile', () => {
        it('returns user profile', async () => {
            const mockUser = { id: 'u-1', full_name: 'Test User', role: 'manager' };
            vi.spyOn(supabase, 'from').mockReturnValue(mockChain(mockUser) as never);

            const result = await userService.getUserProfile('u-1');
            expect(result).toEqual(mockUser);
        });

        it('throws on error', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue(
                mockChain(null, { message: 'Not found' }) as never
            );
            await expect(userService.getUserProfile('bad-id')).rejects.toBeDefined();
        });
    });

    describe('getOrchardUsers', () => {
        it('returns users for orchard', async () => {
            const users = [
                { id: 'u-1', full_name: 'Alice', role: 'manager' },
                { id: 'u-2', full_name: 'Bob', role: 'team_leader' },
            ];
            vi.spyOn(supabase, 'from').mockReturnValue(mockChain(users) as never);

            const result = await userService.getOrchardUsers('o-1');
            expect(result).toHaveLength(2);
        });

        it('returns empty array when no users', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue(mockChain(null) as never);
            const result = await userService.getOrchardUsers('o-1');
            expect(result).toEqual([]);
        });
    });

    describe('getAvailableTeamLeaders', () => {
        it('returns team leaders', async () => {
            const leaders = [{ id: 'tl-1', full_name: 'Leader', role: 'team_leader' }];
            vi.spyOn(supabase, 'from').mockReturnValue(mockChain(leaders) as never);

            const result = await userService.getAvailableTeamLeaders();
            expect(result).toHaveLength(1);
        });
    });

    describe('getAvailableRunners', () => {
        it('returns runners', async () => {
            const runners = [{ id: 'r-1', full_name: 'Runner', role: 'runner' }];
            vi.spyOn(supabase, 'from').mockReturnValue(mockChain(runners) as never);

            const result = await userService.getAvailableRunners();
            expect(result).toHaveLength(1);
        });
    });

    describe('assignUserToOrchard', () => {
        it('throws if userId is empty', async () => {
            await expect(userService.assignUserToOrchard('', 'o-1'))
                .rejects.toThrow('User ID is required');
        });

        it('throws if orchardId is empty', async () => {
            await expect(userService.assignUserToOrchard('u-1', ''))
                .rejects.toThrow('Orchard ID is required');
        });
    });

    describe('unassignUserFromOrchard', () => {
        it('throws if userId is empty', async () => {
            await expect(userService.unassignUserFromOrchard(''))
                .rejects.toThrow('User ID is required');
        });
    });
});
