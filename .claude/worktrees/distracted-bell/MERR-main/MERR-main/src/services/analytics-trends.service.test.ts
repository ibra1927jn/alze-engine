/**
 * Tests for analytics-trends.service.ts
 * Covers: getRowDensity, getDailyTrends (demo path), getDailyBleed
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';

// Mock supabase
vi.mock('./supabase', () => ({
    supabase: {
        from: vi.fn(() => ({
            select: vi.fn().mockReturnThis(),
            eq: vi.fn().mockReturnThis(),
            gte: vi.fn().mockReturnThis(),
            lte: vi.fn().mockReturnThis(),
            order: vi.fn().mockReturnThis(),
        })),
    },
}));

vi.mock('@/utils/logger', () => ({
    logger: { error: vi.fn(), warn: vi.fn(), info: vi.fn() },
}));

import { AnalyticsTrendsService } from './analytics-trends.service';
import { supabase } from './supabase';

describe('AnalyticsTrendsService', () => {
    let service: AnalyticsTrendsService;

    beforeEach(() => {
        service = new AnalyticsTrendsService();
        vi.clearAllMocks();
    });

    describe('getRowDensity', () => {
        it('returns empty result when no events', async () => {
            const chain = { select: vi.fn().mockReturnThis(), eq: vi.fn().mockReturnThis(), gte: vi.fn().mockReturnThis(), lte: vi.fn().mockResolvedValue({ data: [], error: null }) };
            vi.mocked(supabase.from).mockReturnValue(chain as any);

            const result = await service.getRowDensity('orch-1', '2026-03-01', '2026-03-03');
            expect(result.total_buckets).toBe(0);
            expect(result.density_by_row).toEqual([]);
            expect(result.top_rows).toEqual([]);
            expect(result.pending_rows).toEqual([]);
        });

        it('throws on supabase error', async () => {
            const chain = { select: vi.fn().mockReturnThis(), eq: vi.fn().mockReturnThis(), gte: vi.fn().mockReturnThis(), lte: vi.fn().mockResolvedValue({ data: null, error: { message: 'DB error' } }) };
            vi.mocked(supabase.from).mockReturnValue(chain as any);

            await expect(service.getRowDensity('orch-1', '2026-03-01', '2026-03-03')).rejects.toEqual({ message: 'DB error' });
        });

        it('calculates density correctly with events', async () => {
            const events = [
                { row_number: 1, picker_id: 'p1', scanned_at: '2026-03-01T08:00:00' },
                { row_number: 1, picker_id: 'p1', scanned_at: '2026-03-01T09:00:00' },
                { row_number: 1, picker_id: 'p2', scanned_at: '2026-03-01T09:30:00' },
                { row_number: 2, picker_id: 'p1', scanned_at: '2026-03-01T10:00:00' },
            ];
            const chain = { select: vi.fn().mockReturnThis(), eq: vi.fn().mockReturnThis(), gte: vi.fn().mockReturnThis(), lte: vi.fn().mockResolvedValue({ data: events, error: null }) };
            vi.mocked(supabase.from).mockReturnValue(chain as any);

            const result = await service.getRowDensity('orch-1', '2026-03-01', '2026-03-03', 100);
            expect(result.total_buckets).toBe(4);
            expect(result.total_rows_harvested).toBe(2);
            expect(result.density_by_row).toHaveLength(2);

            const row1 = result.density_by_row.find(r => r.row_number === 1)!;
            expect(row1.total_buckets).toBe(3);
            expect(row1.unique_pickers).toBe(2);
            expect(row1.target_completion).toBe(3); // 3/100 * 100

            // Row 1 and 2 both below 50% → pending
            expect(result.pending_rows).toContain(1);
            expect(result.pending_rows).toContain(2);
        });

        it('identifies top rows (>=100% completion)', async () => {
            const events = Array.from({ length: 100 }, (_, i) => ({
                row_number: 5, picker_id: `p${i % 3}`, scanned_at: `2026-03-01T${String(8 + (i % 8)).padStart(2, '0')}:00:00`,
            }));
            const chain = { select: vi.fn().mockReturnThis(), eq: vi.fn().mockReturnThis(), gte: vi.fn().mockReturnThis(), lte: vi.fn().mockResolvedValue({ data: events, error: null }) };
            vi.mocked(supabase.from).mockReturnValue(chain as any);

            const result = await service.getRowDensity('orch-1', '2026-03-01', '2026-03-03', 100);
            expect(result.top_rows).toContain(5);
        });
    });

    describe('getDailyTrends', () => {
        it('returns demo data when no closures', async () => {
            const chain = { select: vi.fn().mockReturnThis(), eq: vi.fn().mockReturnThis(), gte: vi.fn().mockReturnThis(), order: vi.fn().mockResolvedValue({ data: [], error: null }) };
            vi.mocked(supabase.from).mockReturnValue(chain as any);

            const result = await service.getDailyTrends('orch-1', 3);
            expect(result.costPerBin).toHaveLength(3);
            expect(result.totalBins).toHaveLength(3);
            expect(result.workforceSize).toHaveLength(3);
            expect(result.breakEvenCost).toBe(8.50);
        });

        it('handles supabase error gracefully', async () => {
            const chain = { select: vi.fn().mockReturnThis(), eq: vi.fn().mockReturnThis(), gte: vi.fn().mockReturnThis(), order: vi.fn().mockRejectedValue(new Error('network')) };
            vi.mocked(supabase.from).mockReturnValue(chain as any);

            const result = await service.getDailyTrends('orch-1', 7);
            // Falls back to demo data
            expect(result.costPerBin).toHaveLength(7);
        });
    });

    describe('getDailyBleed', () => {
        it('returns array with correct length', async () => {
            const result = await service.getDailyBleed('orch-1', 5);
            expect(result).toHaveLength(5);
        });

        it('labels last day as Today', async () => {
            const result = await service.getDailyBleed(undefined, 3);
            expect(result[result.length - 1].label).toBe('Today');
        });

        it('all values are non-negative', async () => {
            const result = await service.getDailyBleed(undefined, 7);
            result.forEach(d => expect(d.value).toBeGreaterThanOrEqual(0));
        });
    });
});
