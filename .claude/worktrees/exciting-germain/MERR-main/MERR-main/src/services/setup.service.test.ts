/**
 * setup.service.test.ts — Unit tests for orchard setup service
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn() },
}));

import { supabase } from './supabase';
import { createOrchardSetup } from './setup.service';
import type { OrchardSetupData } from './setup.service';

const validSetupData: OrchardSetupData = {
    orchard: { code: 'JP-01', name: 'JP Cherries', location: 'Central Otago', total_rows: 50 },
    teams: [{ name: 'Team Alpha', leader_name: 'John', max_pickers: 20 }],
    rates: { variety: 'Cherry', piece_rate: 2.5, start_time: '07:00' },
};

describe('createOrchardSetup', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
    });

    it('succeeds via RPC when available', async () => {
        vi.spyOn(supabase, 'rpc').mockResolvedValue({
            data: { id: 'o-1', code: 'JP-01', name: 'JP Cherries' },
            error: null,
        } as never);

        const result = await createOrchardSetup(validSetupData);
        expect(result.ok).toBe(true);
        if (result.ok) {
            expect(result.data.code).toBe('JP-01');
        }
    });

    it('falls back to sequential inserts when RPC not deployed (42883)', async () => {
        // RPC not found
        vi.spyOn(supabase, 'rpc').mockResolvedValue({
            data: null,
            error: { code: '42883', message: 'function not found' },
        } as never);

        const mockFrom = vi.fn();
        // orchards insert
        mockFrom.mockReturnValueOnce({
            insert: vi.fn().mockReturnValue({
                select: vi.fn().mockReturnValue({
                    single: vi.fn().mockResolvedValue({
                        data: { id: 'o-1', code: 'JP-01', name: 'JP Cherries' },
                        error: null,
                    }),
                }),
            }),
        });
        // day_setups insert
        mockFrom.mockReturnValueOnce({
            insert: vi.fn().mockResolvedValue({ error: null }),
        });
        vi.spyOn(supabase, 'from').mockImplementation(mockFrom);

        const result = await createOrchardSetup(validSetupData);
        expect(result.ok).toBe(true);
    });

    it('returns error when RPC fails with non-42883 error', async () => {
        vi.spyOn(supabase, 'rpc').mockResolvedValue({
            data: null,
            error: { code: '42501', message: 'Permission denied' },
        } as never);

        const result = await createOrchardSetup(validSetupData);
        expect(result.ok).toBe(false);
        if (!result.ok) {
            expect(result.error.code).toBe('RPC_FAILED');
        }
    });

    it('returns error when orchard insert fails in fallback', async () => {
        vi.spyOn(supabase, 'rpc').mockResolvedValue({
            data: null,
            error: { code: '42883', message: 'not found' },
        } as never);

        vi.spyOn(supabase, 'from').mockReturnValue({
            insert: vi.fn().mockReturnValue({
                select: vi.fn().mockReturnValue({
                    single: vi.fn().mockResolvedValue({
                        data: null,
                        error: { message: 'Duplicate code' },
                    }),
                }),
            }),
        } as never);

        const result = await createOrchardSetup(validSetupData);
        expect(result.ok).toBe(false);
        if (!result.ok) {
            expect(result.error.code).toBe('ORCHARD_CREATE_FAILED');
        }
    });

    it('still succeeds if day_setup insert fails (non-blocking)', async () => {
        vi.spyOn(supabase, 'rpc').mockResolvedValue({
            data: null,
            error: { code: '42883', message: 'not found' },
        } as never);

        const mockFrom = vi.fn();
        mockFrom.mockReturnValueOnce({
            insert: vi.fn().mockReturnValue({
                select: vi.fn().mockReturnValue({
                    single: vi.fn().mockResolvedValue({
                        data: { id: 'o-1', code: 'JP-01', name: 'JP Cherries' },
                        error: null,
                    }),
                }),
            }),
        });
        mockFrom.mockReturnValueOnce({
            insert: vi.fn().mockResolvedValue({ error: { message: 'constraint violation' } }),
        });
        vi.spyOn(supabase, 'from').mockImplementation(mockFrom);

        const result = await createOrchardSetup(validSetupData);
        // Still succeeds — day_setup failure is non-blocking
        expect(result.ok).toBe(true);
    });

    it('catches unexpected errors and returns Result.Err', async () => {
        vi.spyOn(supabase, 'rpc').mockRejectedValue(new Error('Network crash'));

        const result = await createOrchardSetup(validSetupData);
        expect(result.ok).toBe(false);
        if (!result.ok) {
            expect(result.error.code).toBe('SETUP_UNEXPECTED_ERROR');
        }
    });
});
