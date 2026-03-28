/**
 * bucketSlice Tests — pure validation and state logic
 */
import { describe, it, expect } from 'vitest';

// Test the pure validation logic extracted from bucketSlice
const MAX_ALLOWED_SKEW = 5 * 60 * 1000; // 5 minutes in ms

interface Picker { id: string; status: string; checked_in_today: boolean; }
interface ScannedBucket {
    id: string; picker_id: string; synced: boolean;
    quality_grade: string; timestamp: string; orchard_id: string;
}

const shouldRejectArchived = (picker: Picker | undefined) =>
    picker?.status === 'archived';

const shouldWarnNotCheckedIn = (picker: Picker | undefined) =>
    picker && !picker.checked_in_today;

const isClockSkewTooHigh = (skewMs: number) =>
    Math.abs(skewMs) > MAX_ALLOWED_SKEW;

const markBucketSynced = (buckets: ScannedBucket[], id: string) =>
    buckets.map(b => b.id === id ? { ...b, synced: true } : b);

const clearSyncedBuckets = (buckets: ScannedBucket[]) =>
    buckets.filter(b => !b.synced);

describe('bucketSlice — validation', () => {
    it('rejects archived picker', () => {
        expect(shouldRejectArchived({ id: 'p1', status: 'archived', checked_in_today: true })).toBe(true);
    });

    it('allows active picker', () => {
        expect(shouldRejectArchived({ id: 'p1', status: 'active', checked_in_today: true })).toBe(false);
    });

    it('allows when picker is undefined (new)', () => {
        expect(shouldRejectArchived(undefined)).toBe(false);
    });

    it('warns on not checked-in picker', () => {
        expect(shouldWarnNotCheckedIn({ id: 'p1', status: 'active', checked_in_today: false })).toBe(true);
    });

    it('does not warn when checked in', () => {
        expect(shouldWarnNotCheckedIn({ id: 'p1', status: 'active', checked_in_today: true })).toBe(false);
    });
});

describe('bucketSlice — clock skew', () => {
    it('rejects skew > 5 minutes', () => {
        expect(isClockSkewTooHigh(6 * 60 * 1000)).toBe(true);
    });

    it('allows skew within 5 minutes', () => {
        expect(isClockSkewTooHigh(3 * 60 * 1000)).toBe(false);
    });

    it('rejects negative skew > 5 minutes', () => {
        expect(isClockSkewTooHigh(-6 * 60 * 1000)).toBe(true);
    });

    it('allows exactly 5 minutes', () => {
        expect(isClockSkewTooHigh(5 * 60 * 1000)).toBe(false);
    });
});

describe('bucketSlice — sync state', () => {
    const buckets: ScannedBucket[] = [
        { id: 'b1', picker_id: 'p1', synced: false, quality_grade: 'A', timestamp: '', orchard_id: 'o1' },
        { id: 'b2', picker_id: 'p2', synced: false, quality_grade: 'B', timestamp: '', orchard_id: 'o1' },
        { id: 'b3', picker_id: 'p3', synced: true, quality_grade: 'A', timestamp: '', orchard_id: 'o1' },
    ];

    it('marks specific bucket as synced', () => {
        const updated = markBucketSynced(buckets, 'b1');
        expect(updated.find(b => b.id === 'b1')?.synced).toBe(true);
        expect(updated.find(b => b.id === 'b2')?.synced).toBe(false);
    });

    it('clears all synced buckets', () => {
        const cleared = clearSyncedBuckets(buckets);
        expect(cleared).toHaveLength(2);
        expect(cleared.every(b => !b.synced)).toBe(true);
    });

    it('clearSynced on all-unsynced returns all', () => {
        const unsynced = buckets.filter(b => !b.synced);
        expect(clearSyncedBuckets(unsynced)).toHaveLength(2);
    });

    it('clearSynced on empty returns empty', () => {
        expect(clearSyncedBuckets([])).toEqual([]);
    });
});
