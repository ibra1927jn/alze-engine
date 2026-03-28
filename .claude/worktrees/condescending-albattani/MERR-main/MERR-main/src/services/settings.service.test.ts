import { describe, it, expect, vi, beforeEach } from 'vitest';
import { settingsService } from './settings.service';

// ── Mocks ──────────────────────────────────
vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn() },
}));
vi.mock('./supabase', () => ({
    supabase: { from: vi.fn() },
}));

import { supabase } from './supabase';

const mockFrom = supabase.from as ReturnType<typeof vi.fn>;

describe('settingsService', () => {
    beforeEach(() => vi.clearAllMocks());

    // ═══════════════════════════════════════
    // getHarvestSettings
    // ═══════════════════════════════════════

    it('getHarvestSettings returns mapped settings on success', async () => {
        const dbRow = {
            min_wage_rate: 23.15,
            piece_rate: 5.0,
            min_buckets_per_hour: 3,
            target_tons: 100,
            variety: 'Hayward',
        };
        mockFrom.mockReturnValue({
            select: () => ({ eq: () => ({ single: () => ({ data: dbRow, error: null }) }) }),
        });

        const result = await settingsService.getHarvestSettings('orchard-1');

        expect(result).toEqual({
            min_wage_rate: 23.15,
            piece_rate: 5.0,
            min_buckets_per_hour: 3,
            target_tons: 100,
            variety: 'Hayward',
        });
    });

    it('getHarvestSettings returns null on error', async () => {
        mockFrom.mockReturnValue({
            select: () => ({ eq: () => ({ single: () => ({ data: null, error: { message: 'Not found' } }) }) }),
        });

        const result = await settingsService.getHarvestSettings('bad-id');
        expect(result).toBeNull();
    });

    // ═══════════════════════════════════════
    // updateHarvestSettings
    // ═══════════════════════════════════════

    it('updateHarvestSettings returns true and upserts with orchard_id and onConflict', async () => {
        const mockUpsert = vi.fn().mockReturnValue({ error: null });
        mockFrom.mockReturnValue({ upsert: mockUpsert });

        const updates = { piece_rate: 6.0, variety: 'Gold3' };
        const result = await settingsService.updateHarvestSettings('orchard-1', updates);

        expect(result).toBe(true);
        expect(mockUpsert).toHaveBeenCalledWith(
            { orchard_id: 'orchard-1', piece_rate: 6.0, variety: 'Gold3' },
            { onConflict: 'orchard_id' },
        );
    });

    it('updateHarvestSettings returns false on error', async () => {
        mockFrom.mockReturnValue({
            upsert: () => ({ error: { message: 'Permission denied' } }),
        });

        const result = await settingsService.updateHarvestSettings('orchard-1', {});
        expect(result).toBe(false);
    });

    it('updateHarvestSettings does NOT include client-side updated_at (R9-Fix3)', async () => {
        const mockUpsert = vi.fn().mockReturnValue({ error: null });
        mockFrom.mockReturnValue({ upsert: mockUpsert });

        await settingsService.updateHarvestSettings('o1', { piece_rate: 7 });

        const upsertedData = mockUpsert.mock.calls[0][0];
        expect(upsertedData).not.toHaveProperty('updated_at');
    });
});
