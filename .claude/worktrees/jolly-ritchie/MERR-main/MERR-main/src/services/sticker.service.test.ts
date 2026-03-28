/**
 * sticker.service.test.ts — Unit tests for sticker scanning service
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn() },
}));

vi.mock('@/utils/nzst', () => ({
    todayNZST: vi.fn(() => '2026-03-04'),
    toNZST: vi.fn((d: Date) => d.toISOString()),
}));

import { supabase } from './supabase';
import {
    extractPickerIdFromSticker,
    checkStickerScanned,
    scanSticker,
    getTeamLeaderStats,
    getTodayBucketsByPicker,
} from './sticker.service';

describe('stickerService', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
    });

    // ── extractPickerIdFromSticker (pure function) ──
    describe('extractPickerIdFromSticker', () => {
        it('extracts first 5 digits from sticker code', () => {
            expect(extractPickerIdFromSticker('2662200498')).toBe('26622');
        });

        it('strips non-digit characters before extracting', () => {
            expect(extractPickerIdFromSticker('ABC26622-0498')).toBe('26622');
        });

        it('returns null for codes with fewer than 5 digits', () => {
            expect(extractPickerIdFromSticker('1234')).toBeNull();
            expect(extractPickerIdFromSticker('AB12')).toBeNull();
        });

        it('handles exactly 5 digit code', () => {
            expect(extractPickerIdFromSticker('12345')).toBe('12345');
        });

        it('returns null for empty string', () => {
            expect(extractPickerIdFromSticker('')).toBeNull();
        });
    });

    // ── checkStickerScanned ──
    describe('checkStickerScanned', () => {
        it('returns true when sticker exists in DB', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        maybeSingle: vi.fn().mockResolvedValue({ data: { id: 's-1' }, error: null }),
                    }),
                }),
            } as never);

            expect(await checkStickerScanned('2662200498')).toBe(true);
        });

        it('returns false when sticker not found', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        maybeSingle: vi.fn().mockResolvedValue({ data: null, error: null }),
                    }),
                }),
            } as never);

            expect(await checkStickerScanned('9999900001')).toBe(false);
        });

        it('throws on supabase error', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        maybeSingle: vi.fn().mockResolvedValue({
                            data: null,
                            error: { message: 'Connection failed' },
                        }),
                    }),
                }),
            } as never);

            await expect(checkStickerScanned('0000000001')).rejects.toThrow();
        });
    });

    // ── scanSticker ──
    describe('scanSticker', () => {
        it('returns success on valid scan', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                insert: vi.fn().mockReturnValue({
                    select: vi.fn().mockReturnValue({
                        single: vi.fn().mockResolvedValue({
                            data: { id: 's-1', sticker_code: '2662200498' },
                            error: null,
                        }),
                    }),
                }),
            } as never);

            const result = await scanSticker('2662200498', 'bin-1', 'user-1');
            expect(result.success).toBe(true);
            expect(result.pickerId).toBe('26622');
            expect(result.sticker).toBeDefined();
        });

        it('returns error for invalid sticker code (< 5 digits)', async () => {
            const result = await scanSticker('1234', 'bin-1');
            expect(result.success).toBe(false);
            expect(result.error).toContain('Código QR inválido');
        });

        it('handles duplicate sticker (23505 constraint)', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                insert: vi.fn().mockReturnValue({
                    select: vi.fn().mockReturnValue({
                        single: vi.fn().mockResolvedValue({
                            data: null,
                            error: { code: '23505', message: 'Unique violation' },
                        }),
                    }),
                }),
            } as never);

            const result = await scanSticker('2662200498', 'bin-1');
            expect(result.success).toBe(false);
            expect(result.error).toContain('ya fue escaneado');
        });

        it('normalizes sticker code (trim + uppercase)', async () => {
            const mockInsert = vi.fn().mockReturnValue({
                select: vi.fn().mockReturnValue({
                    single: vi.fn().mockResolvedValue({
                        data: { id: 's-1', sticker_code: '2662200498' },
                        error: null,
                    }),
                }),
            });
            vi.spyOn(supabase, 'from').mockReturnValue({ insert: mockInsert } as never);

            await scanSticker('  2662200498  ', 'bin-1');
            const insertedData = mockInsert.mock.calls[0][0][0];
            expect(insertedData.sticker_code).toBe('2662200498');
        });

        it('returns OFFLINE_MODE error on network failure', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                insert: vi.fn().mockReturnValue({
                    select: vi.fn().mockReturnValue({
                        single: vi.fn().mockResolvedValue({
                            data: null,
                            error: { message: 'Failed to fetch' },
                        }),
                    }),
                }),
            } as never);

            const result = await scanSticker('2662200498', 'bin-1');
            expect(result.success).toBe(false);
            expect(result.error).toBe('OFFLINE_MODE');
        });
    });

    // ── getTeamLeaderStats ──
    describe('getTeamLeaderStats', () => {
        it('returns total and today counts', async () => {
            const mockFrom = vi.fn();
            // Total count
            mockFrom.mockReturnValueOnce({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockResolvedValue({ count: 150 }),
                }),
            });
            // Today count
            mockFrom.mockReturnValueOnce({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        gte: vi.fn().mockReturnValue({
                            lte: vi.fn().mockResolvedValue({ count: 25 }),
                        }),
                    }),
                }),
            });
            vi.spyOn(supabase, 'from').mockImplementation(mockFrom);

            const stats = await getTeamLeaderStats('tl-1');
            expect(stats.totalBuckets).toBe(150);
            expect(stats.todayBuckets).toBe(25);
        });

        it('returns zeroes on error', async () => {
            vi.spyOn(supabase, 'from').mockImplementation(() => {
                throw new Error('DB down');
            });

            const stats = await getTeamLeaderStats('tl-1');
            expect(stats).toEqual({ totalBuckets: 0, todayBuckets: 0 });
        });
    });

    // ── getTodayBucketsByPicker ──
    describe('getTodayBucketsByPicker', () => {
        it('returns count for today', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        gte: vi.fn().mockReturnValue({
                            lte: vi.fn().mockResolvedValue({ count: 12 }),
                        }),
                    }),
                }),
            } as never);

            expect(await getTodayBucketsByPicker('picker-1')).toBe(12);
        });

        it('returns 0 on error', async () => {
            vi.spyOn(supabase, 'from').mockImplementation(() => {
                throw new Error('DB down');
            });

            expect(await getTodayBucketsByPicker('picker-1')).toBe(0);
        });
    });
});
