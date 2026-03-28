/**
 * Tests for storeTypes — Type shape validation
 * These tests validate that the type interfaces are correctly defined
 * by creating valid instances and verifying they compile and have the expected shape.
 */
import { describe, it, expect } from 'vitest';
import type {
    ScannedBucket,
    HarvestStats,
    SettingsSlice,
    CrewSlice,
    BucketSlice,
    IntelligenceSlice,
    RowSlice,
    OrchestratorSlice,
    HarvestStoreState,
} from './storeTypes';

describe('storeTypes — Shape Validation', () => {
    it('ScannedBucket has all required fields', () => {
        const bucket: ScannedBucket = {
            id: 'b1',
            picker_id: 'p1',
            quality_grade: 'A',
            timestamp: '2026-03-05T08:00:00Z',
            synced: false,
            orchard_id: 'o1',
        };
        expect(bucket.id).toBe('b1');
        expect(bucket.quality_grade).toBe('A');
        expect(bucket.synced).toBe(false);
    });

    it('HarvestStats has all numeric metrics', () => {
        const stats: HarvestStats = {
            totalBuckets: 100,
            payEstimate: 300,
            tons: 2.5,
            velocity: 12.5,
            goalVelocity: 15,
            binsFull: 5,
        };
        expect(stats.totalBuckets).toBe(100);
        expect(stats.tons).toBe(2.5);
    });

    it('HarvestStoreState is a valid intersection type', () => {
        // Verify the type includes properties from all slices
        const mockState = {} as HarvestStoreState;
        // These keys should exist from their respective slices
        const stateKeys: (keyof HarvestStoreState)[] = [
            'crew', 'buckets', 'settings', 'stats', 'rowAssignments',
            'currentUser', 'inventory', 'orchard', 'simulationMode',
        ];
        // Can't test at runtime since mockState is empty, but verifies types compile
        expect(stateKeys.length).toBe(9);
    });

    it('quality grades are constrained to valid values', () => {
        const validGrades: ScannedBucket['quality_grade'][] = ['A', 'B', 'C', 'reject'];
        expect(validGrades).toHaveLength(4);
    });
});
