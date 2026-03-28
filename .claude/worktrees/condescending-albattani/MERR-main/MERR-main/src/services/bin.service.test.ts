import { describe, it, expect, vi, beforeEach } from 'vitest';
import { binService } from './bin.service';

// ── Mock supabase ──────────────────────────
vi.mock('./supabase', () => ({
    supabase: {
        from: vi.fn(),
    },
}));
vi.mock('@/utils/nzst', () => ({
    nowNZST: () => '2026-02-14T10:00:00+13:00',
}));

import { supabase } from './supabase';

const mockFrom = supabase.from as ReturnType<typeof vi.fn>;

describe('binService', () => {
    beforeEach(() => vi.clearAllMocks());

    // ═══════════════════════════════════════
    // getBins
    // ═══════════════════════════════════════

    it('getBins returns mapped bins on success', async () => {
        const dbRows = [
            { id: 'b1', status: 'full', variety: 'Hayward', bin_code: 'BIN-001' },
            { id: 'b2', status: 'empty', variety: null, bin_code: 'BIN-002' },
        ];
        mockFrom.mockReturnValue({
            select: () => ({ eq: () => ({ data: dbRows, error: null }) }),
        });

        const result = await binService.getBins('orchard-1');

        expect(result).toHaveLength(2);
        expect(result[0]).toEqual({
            id: 'b1', status: 'full', fillPercentage: 0,
            type: 'Hayward', bin_code: 'BIN-001',
        });
        expect(result[1].type).toBe('Standard'); // fallback when variety is null
    });

    it('getBins returns empty array when data is null', async () => {
        mockFrom.mockReturnValue({
            select: () => ({ eq: () => ({ data: null, error: null }) }),
        });

        const result = await binService.getBins('orchard-x');
        expect(result).toEqual([]);
    });

    it('getBins throws on error', async () => {
        mockFrom.mockReturnValue({
            select: () => ({ eq: () => ({ data: null, error: { message: 'DB down' } }) }),
        });

        await expect(binService.getBins('orchard-1')).rejects.toEqual({ message: 'DB down' });
    });

    // ═══════════════════════════════════════
    // updateBinStatus
    // ═══════════════════════════════════════

    it('updateBinStatus sends filled_at when status is full', async () => {
        const mockUpdate = vi.fn().mockReturnValue({
            eq: () => ({ error: null }),
        });
        mockFrom.mockReturnValue({ update: mockUpdate });

        await binService.updateBinStatus('b1', 'full');

        expect(mockUpdate).toHaveBeenCalledWith({
            status: 'full',
            filled_at: '2026-02-14T10:00:00+13:00',
        });
    });

    it('updateBinStatus sends filled_at as null for non-full status', async () => {
        const mockUpdate = vi.fn().mockReturnValue({
            eq: () => ({ error: null }),
        });
        mockFrom.mockReturnValue({ update: mockUpdate });

        await binService.updateBinStatus('b1', 'empty');

        expect(mockUpdate).toHaveBeenCalledWith({
            status: 'empty',
            filled_at: null,
        });
    });

    it('updateBinStatus throws on error', async () => {
        mockFrom.mockReturnValue({
            update: () => ({ eq: () => ({ error: { message: 'Update failed' } }) }),
        });

        await expect(binService.updateBinStatus('b1', 'full')).rejects.toEqual({ message: 'Update failed' });
    });
});
