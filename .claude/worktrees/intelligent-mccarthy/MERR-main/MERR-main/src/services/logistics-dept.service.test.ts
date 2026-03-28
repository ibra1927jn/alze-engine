/**
 * logistics-dept.service.test.ts — Unit tests
 *
 * The service uses logisticsRepository with methods:
 *   getBinStatuses, getFleetStatuses, getPendingRequestStatuses,
 *   getFleetAll, getBinInventory, getActiveRequests, getCompletedRequests,
 *   getVehicleNames
 *
 * Pure functions: mapBinStatus (private), calculateFillPercentage (private)
 * They are private, so we test them indirectly via fetchBinInventory.
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn() },
}));

vi.mock('@/services/sync.service', () => ({
    syncService: { addToQueue: vi.fn().mockResolvedValue('queue-id') },
}));

// Mock logisticsRepository with the CORRECT method names from the source
const mockGetBinStatuses = vi.fn();
const mockGetFleetStatuses = vi.fn();
const mockGetPendingRequestStatuses = vi.fn();
const mockGetFleetAll = vi.fn();
const mockGetBinInventory = vi.fn();
const mockGetActiveRequests = vi.fn();
const mockGetCompletedRequests = vi.fn();
const mockGetVehicleNames = vi.fn();

vi.mock('@/repositories/logistics.repository', () => ({
    logisticsRepository: {
        getBinStatuses: (...args: unknown[]) => mockGetBinStatuses(...args),
        getFleetStatuses: (...args: unknown[]) => mockGetFleetStatuses(...args),
        getPendingRequestStatuses: (...args: unknown[]) => mockGetPendingRequestStatuses(...args),
        getFleetAll: (...args: unknown[]) => mockGetFleetAll(...args),
        getBinInventory: (...args: unknown[]) => mockGetBinInventory(...args),
        getActiveRequests: (...args: unknown[]) => mockGetActiveRequests(...args),
        getCompletedRequests: (...args: unknown[]) => mockGetCompletedRequests(...args),
        getVehicleNames: (...args: unknown[]) => mockGetVehicleNames(...args),
    },
}));

import {
    fetchLogisticsSummary,
    fetchFleet,
    fetchBinInventory,
    fetchTransportRequests,
    fetchTransportHistory,
} from './logistics-dept.service';

describe('logistics-dept.service', () => {
    beforeEach(() => {
        vi.clearAllMocks();
    });

    describe('fetchLogisticsSummary', () => {
        it('aggregates bin, fleet, and request counts', async () => {
            mockGetBinStatuses.mockResolvedValue([
                { status: 'full' }, { status: 'full' },
                { status: 'empty' }, { status: 'empty' }, { status: 'empty' },
                { status: 'collected' },
            ]);
            mockGetFleetStatuses.mockResolvedValue([
                { status: 'active' }, { status: 'active' }, { status: 'idle' },
            ]);
            mockGetPendingRequestStatuses.mockResolvedValue([{ id: '1' }, { id: '2' }]);

            const summary = await fetchLogisticsSummary('o-1');
            expect(summary.fullBins).toBe(2);
            expect(summary.emptyBins).toBe(3);
            expect(summary.binsInTransit).toBe(1); // 'collected' maps to binsInTransit
            expect(summary.activeTractors).toBe(2);
            expect(summary.pendingRequests).toBe(2);
        });

        it('returns zeroes on error', async () => {
            mockGetBinStatuses.mockRejectedValue(new Error('DB error'));
            const summary = await fetchLogisticsSummary('o-1');
            expect(summary).toEqual({
                fullBins: 0, emptyBins: 0, activeTractors: 0,
                pendingRequests: 0, binsInTransit: 0,
            });
        });
    });

    describe('fetchFleet', () => {
        it('maps repository data to Tractor objects', async () => {
            mockGetFleetAll.mockResolvedValue([
                {
                    id: 't-1', name: 'Tractor 1', registration: 'AB123',
                    zone: 'A', driver_id: 'd-1', driver_name: 'John',
                    status: 'active', load_status: 'empty',
                    bins_loaded: 0, max_capacity: 10,
                    fuel_level: 80, wof_expiry: '2026-06-01', updated_at: '2026-03-04T10:00:00Z',
                },
            ]);

            const fleet = await fetchFleet('o-1');
            expect(fleet).toHaveLength(1);
            expect(fleet[0].name).toBe('Tractor 1');
            expect(fleet[0].last_update).toBe('2026-03-04T10:00:00Z');
        });

        it('defaults driver_name to "No driver" when missing', async () => {
            mockGetFleetAll.mockResolvedValue([
                {
                    id: 't-2', name: 'Tractor 2', zone: null, driver_name: null,
                    status: 'idle', load_status: 'empty',
                    bins_loaded: 0, max_capacity: 5, updated_at: '2026-03-04',
                },
            ]);

            const fleet = await fetchFleet('o-1');
            expect(fleet[0].driver_name).toBe('No driver');
            expect(fleet[0].zone).toBe('Unassigned');
        });

        it('returns empty array on error', async () => {
            mockGetFleetAll.mockRejectedValue(new Error('err'));
            const fleet = await fetchFleet('o-1');
            expect(fleet).toEqual([]);
        });
    });

    describe('fetchBinInventory', () => {
        it('maps repository bins with status mapping', async () => {
            mockGetBinInventory.mockResolvedValue([
                { id: 'b-1', bin_code: 'BIN-001', status: 'full', location: { zone: 'B2' }, filled_at: '2026-03-04', created_at: '2026-03-01', variety: 'Royal Gala' },
                { id: 'b-2', bin_code: null, status: 'partial', location: null, filled_at: null, created_at: '2026-03-02', variety: null },
            ]);

            const bins = await fetchBinInventory('o-1');
            expect(bins).toHaveLength(2);
            expect(bins[0].status).toBe('full');
            expect(bins[0].fill_percentage).toBe(100);
            expect(bins[0].zone).toBe('B2');
            expect(bins[1].status).toBe('filling'); // 'partial' → 'filling'
            expect(bins[1].fill_percentage).toBe(50);
        });

        it('returns empty array on error', async () => {
            mockGetBinInventory.mockRejectedValue(new Error('err'));
            expect(await fetchBinInventory('o-1')).toEqual([]);
        });
    });

    describe('fetchTransportRequests', () => {
        it('returns mapped transport requests', async () => {
            mockGetActiveRequests.mockResolvedValue([
                {
                    id: 'tr-1', requested_by: 'u-1', requester_name: 'John',
                    zone: 'A', bins_count: 5, priority: 'high',
                    status: 'pending', assigned_vehicle: null, assigned_by: null,
                    created_at: '2026-03-04T10:00:00Z', completed_at: null,
                    notes: null, updated_at: '2026-03-04T10:00:00Z',
                },
            ]);

            const requests = await fetchTransportRequests('o-1');
            expect(requests).toHaveLength(1);
            expect(requests[0].priority).toBe('high');
        });
    });

    describe('fetchTransportHistory', () => {
        it('computes duration and maps vehicle names', async () => {
            mockGetCompletedRequests.mockResolvedValue([
                {
                    id: 'h-1', assigned_vehicle: 'v-1', requester_name: 'John',
                    zone: 'A', bins_count: 3,
                    created_at: '2026-03-04T10:00:00Z',
                    completed_at: '2026-03-04T10:30:00Z',
                },
            ]);
            mockGetVehicleNames.mockResolvedValue({ 'v-1': 'Big Tractor' });

            const history = await fetchTransportHistory('o-1');
            expect(history).toHaveLength(1);
            expect(history[0].tractor_name).toBe('Big Tractor');
            expect(history[0].duration_minutes).toBe(30);
        });

        it('returns empty array on error', async () => {
            mockGetCompletedRequests.mockRejectedValue(new Error('err'));
            expect(await fetchTransportHistory('o-1')).toEqual([]);
        });
    });
});
