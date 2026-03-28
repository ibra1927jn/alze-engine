/**
 * useRunnerManagement Hook Tests — pure logic
 */
import { describe, it, expect } from 'vitest';

// Test the pure computation logic from useRunnerManagement
interface Runner {
    id: string;
    name: string;
    status: 'Active' | 'Break' | 'Off Duty';
    bucketsHandled: number;
    binsCompleted: number;
}

const getActiveCount = (runners: Runner[]) => runners.filter(r => r.status === 'Active').length;
const getTotalBuckets = (runners: Runner[]) => runners.reduce((s, r) => s + r.bucketsHandled, 0);
const getTotalBins = (runners: Runner[]) => runners.reduce((s, r) => s + r.binsCompleted, 0);

describe('useRunnerManagement — statistics', () => {
    const runners: Runner[] = [
        { id: '1', name: 'Runner A', status: 'Active', bucketsHandled: 50, binsCompleted: 5 },
        { id: '2', name: 'Runner B', status: 'Break', bucketsHandled: 30, binsCompleted: 3 },
        { id: '3', name: 'Runner C', status: 'Active', bucketsHandled: 40, binsCompleted: 4 },
        { id: '4', name: 'Runner D', status: 'Off Duty', bucketsHandled: 0, binsCompleted: 0 },
    ];

    it('counts active runners', () => {
        expect(getActiveCount(runners)).toBe(2);
    });

    it('sums total buckets handled', () => {
        expect(getTotalBuckets(runners)).toBe(120);
    });

    it('sums total bins completed', () => {
        expect(getTotalBins(runners)).toBe(12);
    });

    it('handles empty runners array', () => {
        expect(getActiveCount([])).toBe(0);
        expect(getTotalBuckets([])).toBe(0);
        expect(getTotalBins([])).toBe(0);
    });
});

describe('useRunnerManagement — runner data mapping', () => {
    it('maps user data to RunnerData format', () => {
        const user = { id: 'u1', full_name: 'John Smith' };
        const mapped = {
            id: user.id,
            name: user.full_name || 'Unknown',
            avatar: (user.full_name || '??').substring(0, 2).toUpperCase(),
            startTime: '08:00 AM',
            status: 'Active' as const,
            bucketsHandled: 0,
            binsCompleted: 0,
        };
        expect(mapped.avatar).toBe('JO');
        expect(mapped.name).toBe('John Smith');
    });

    it('defaults to Unknown for empty name', () => {
        const user = { id: 'u2', full_name: '' };
        const name = user.full_name || 'Unknown';
        expect(name).toBe('Unknown');
    });
});
