/**
 * bucket-ledger.service.test.ts — Unit tests
 *
 * Uses vi.spyOn(supabase, 'from') to intercept Supabase calls.
 * Uses REAL Zod validation (safeParse) since @/lib/schemas mock doesn't apply.
 * Therefore test data must use valid UUIDs where the schema requires them.
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { supabase } from './supabase';

vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn() },
}));

import { bucketLedgerService } from './bucket-ledger.service';

// Valid v4 UUIDs for test data (Zod validates orchard_id and scanned_by as UUID)
const VALID_ORCHARD = 'a1111111-1111-4111-a111-111111111111';
const VALID_RUNNER = 'b2222222-2222-4222-b222-222222222222';
const VALID_PICKER = 'c3333333-3333-4333-8333-333333333333';

describe('bucketLedgerService', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
    });

    describe('recordBucket', () => {
        it('rejects empty picker_id (Zod failure)', async () => {
            await expect(
                bucketLedgerService.recordBucket({
                    picker_id: '',
                    orchard_id: VALID_ORCHARD,
                    scanned_by: VALID_RUNNER,
                } as never)
            ).rejects.toThrow('DATOS INVÁLIDOS');
        });

        it('records directly when picker_id is UUID', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                insert: vi.fn().mockReturnValue({
                    select: vi.fn().mockReturnValue({
                        single: vi.fn().mockResolvedValue({
                            data: { id: 'rec-1', picker_id: VALID_PICKER },
                            error: null,
                        }),
                    }),
                }),
            } as never);

            const result = await bucketLedgerService.recordBucket({
                picker_id: VALID_PICKER,
                orchard_id: VALID_ORCHARD,
                scanned_by: VALID_RUNNER,
                row_number: 1,
            } as never);

            expect(result).toEqual({ id: 'rec-1', picker_id: VALID_PICKER });
        });

        it('resolves badge ID to picker UUID via exact match', async () => {
            const badgeId = 'PKR001';

            const fromSpy = vi.spyOn(supabase, 'from');

            // First call: picker lookup
            fromSpy.mockReturnValueOnce({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        eq: vi.fn().mockReturnValue({
                            maybeSingle: vi.fn().mockResolvedValue({
                                data: { id: VALID_PICKER, picker_id: badgeId },
                                error: null,
                            }),
                        }),
                    }),
                }),
            } as never);

            // Second call: insert bucket record
            fromSpy.mockReturnValueOnce({
                insert: vi.fn().mockReturnValue({
                    select: vi.fn().mockReturnValue({
                        single: vi.fn().mockResolvedValue({
                            data: { id: 'rec-1', picker_id: VALID_PICKER },
                            error: null,
                        }),
                    }),
                }),
            } as never);

            const result = await bucketLedgerService.recordBucket({
                picker_id: badgeId,
                orchard_id: VALID_ORCHARD,
                scanned_by: VALID_RUNNER,
                row_number: 1,
            } as never);

            expect(result.picker_id).toBe(VALID_PICKER);
        });

        it('throws when badge ID has no exact match', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        eq: vi.fn().mockReturnValue({
                            maybeSingle: vi.fn().mockResolvedValue({ data: null, error: null }),
                        }),
                    }),
                }),
            } as never);

            await expect(
                bucketLedgerService.recordBucket({
                    picker_id: 'UNKNOWN',
                    orchard_id: VALID_ORCHARD,
                    scanned_by: VALID_RUNNER,
                    row_number: 1,
                } as never)
            ).rejects.toThrow('CÓDIGO DESCONOCIDO');
        });

        it('throws on Supabase insert error', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                insert: vi.fn().mockReturnValue({
                    select: vi.fn().mockReturnValue({
                        single: vi.fn().mockResolvedValue({
                            data: null,
                            error: { message: 'RLS violation', code: '42501' },
                        }),
                    }),
                }),
            } as never);

            await expect(
                bucketLedgerService.recordBucket({
                    picker_id: VALID_PICKER,
                    orchard_id: VALID_ORCHARD,
                    scanned_by: VALID_RUNNER,
                } as never)
            ).rejects.toBeDefined();
        });
    });

    describe('getPickerHistory', () => {
        it('returns bucket records for a picker', async () => {
            const records = [
                { id: 'r-1', scanned_at: '2026-03-04T10:00:00Z' },
                { id: 'r-2', scanned_at: '2026-03-04T09:00:00Z' },
            ];

            vi.spyOn(supabase, 'from').mockReturnValue({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        order: vi.fn().mockReturnValue({
                            limit: vi.fn().mockResolvedValue({ data: records, error: null }),
                        }),
                    }),
                }),
            } as never);

            const result = await bucketLedgerService.getPickerHistory('p-1');
            expect(result).toHaveLength(2);
        });

        it('throws on error', async () => {
            vi.spyOn(supabase, 'from').mockReturnValue({
                select: vi.fn().mockReturnValue({
                    eq: vi.fn().mockReturnValue({
                        order: vi.fn().mockReturnValue({
                            limit: vi.fn().mockResolvedValue({
                                data: null,
                                error: { message: 'error' },
                            }),
                        }),
                    }),
                }),
            } as never);

            await expect(bucketLedgerService.getPickerHistory('p-1')).rejects.toBeDefined();
        });
    });
});
