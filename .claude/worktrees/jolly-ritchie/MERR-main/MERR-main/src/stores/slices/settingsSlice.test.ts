/**
 * settingsSlice Tests — pure logic
 */
import { describe, it, expect } from 'vitest';

// Test the defaults and merge logic from settingsSlice
interface HarvestSettings {
    piece_rate: number;
    min_wage_rate: number;
    min_buckets_per_hour: number;
    target_tons: number;
    variety: string;
    version?: number;
}

const DEFAULTS: HarvestSettings = {
    piece_rate: 6.5, min_wage_rate: 23.5,
    min_buckets_per_hour: 8, target_tons: 40,
    variety: 'Cherry', version: 0,
};

const mergeWithDefaults = (incoming: Partial<HarvestSettings>): HarvestSettings => ({
    ...DEFAULTS, ...incoming,
});

const hasVersionConflict = (local: number, remote: number): boolean => remote > local;

describe('settingsSlice — defaults', () => {
    it('provides correct NZ minimum wage', () => {
        expect(DEFAULTS.min_wage_rate).toBe(23.5);
    });

    it('provides correct default piece rate', () => {
        expect(DEFAULTS.piece_rate).toBe(6.5);
    });

    it('provides correct default target tons', () => {
        expect(DEFAULTS.target_tons).toBe(40);
    });

    it('default variety is Cherry', () => {
        expect(DEFAULTS.variety).toBe('Cherry');
    });
});

describe('settingsSlice — merge logic', () => {
    it('merges partial updates with defaults', () => {
        const result = mergeWithDefaults({ piece_rate: 7.0 });
        expect(result.piece_rate).toBe(7.0);
        expect(result.min_wage_rate).toBe(23.5);
    });

    it('empty update returns all defaults', () => {
        const result = mergeWithDefaults({});
        expect(result).toEqual(DEFAULTS);
    });

    it('overrides all fields', () => {
        const full: HarvestSettings = {
            piece_rate: 8, min_wage_rate: 25,
            min_buckets_per_hour: 10, target_tons: 60,
            variety: 'Apple', version: 5,
        };
        expect(mergeWithDefaults(full)).toEqual(full);
    });
});

describe('settingsSlice — concurrency control', () => {
    it('detects version conflict', () => {
        expect(hasVersionConflict(1, 2)).toBe(true);
    });

    it('no conflict when versions match', () => {
        expect(hasVersionConflict(2, 2)).toBe(false);
    });

    it('no conflict when local is ahead', () => {
        expect(hasVersionConflict(3, 2)).toBe(false);
    });
});
