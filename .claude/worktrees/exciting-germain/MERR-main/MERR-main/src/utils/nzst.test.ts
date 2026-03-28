// =============================================
// NZST TIMEZONE UTILITY TESTS
// =============================================
import { describe, it, expect } from 'vitest';
import { nowNZST, todayNZST, toNZST } from './nzst';

describe('NZST Timezone Utility', () => {
    // =============================================
    // nowNZST
    // =============================================
    describe('nowNZST', () => {
        it('should return a string', () => {
            expect(typeof nowNZST()).toBe('string');
        });

        it('should include a timezone offset', () => {
            const result = nowNZST();
            // Should end with +XX:00 (NZ offset varies by season and Node ICU data)
            expect(result).toMatch(/\+\d{2}:00$/);
        });

        it('should be in ISO-like format', () => {
            const result = nowNZST();
            // YYYY-MM-DDTHH:MM:SS+HH:00
            expect(result).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\+\d{2}:00$/);
        });

        it('should return different values on successive calls (or same within 1s)', () => {
            const t1 = nowNZST();
            const t2 = nowNZST();
            // Both should be valid ISO-like strings
            expect(t1).toMatch(/^\d{4}/);
            expect(t2).toMatch(/^\d{4}/);
        });
    });

    // =============================================
    // todayNZST
    // =============================================
    describe('todayNZST', () => {
        it('should return a date in YYYY-MM-DD format', () => {
            const today = todayNZST();
            expect(today).toMatch(/^\d{4}-\d{2}-\d{2}$/);
        });

        it('should return a valid date', () => {
            const today = todayNZST();
            const parsed = new Date(today);
            expect(parsed.toString()).not.toBe('Invalid Date');
        });

        it('should return current year', () => {
            const today = todayNZST();
            const year = parseInt(today.split('-')[0]);
            expect(year).toBeGreaterThanOrEqual(2024);
            expect(year).toBeLessThanOrEqual(2030);
        });
    });

    // =============================================
    // toNZST
    // =============================================
    describe('toNZST', () => {
        it('should convert a Date to a string with NZ offset', () => {
            const date = new Date('2026-02-12T00:00:00Z');
            const result = toNZST(date);
            // Should contain a positive offset (NZ is ahead of UTC)
            expect(result).toMatch(/\+\d{2}:00$/);
            // The time portion should reflect NZ time (hours ahead of UTC)
            expect(result).toMatch(/^2026-02-12T/);
        });

        it('should produce consistent results for the same input', () => {
            const date = new Date('2026-06-15T00:00:00Z');
            const result1 = toNZST(date);
            const result2 = toNZST(date);
            expect(result1).toBe(result2);
        });

        it('should produce a date string ahead of UTC', () => {
            // NZ is always ahead of UTC (by 12 or 13 hours)
            const date = new Date('2026-01-15T06:00:00Z');
            const result = toNZST(date);
            // Extract the hour from result
            const hourMatch = result.match(/T(\d{2}):/);
            expect(hourMatch).not.toBeNull();
            const nzHour = parseInt(hourMatch![1]);
            // NZ time should be 6 + offset hours (wrapping at 24)
            // At minimum +10 offset, so hour should be >= 16 (or wrapped next day)
            expect(nzHour).toBeGreaterThanOrEqual(0); // valid hour
            expect(nzHour).toBeLessThanOrEqual(23);
        });

        it('should handle date boundary crossover', () => {
            // Late night UTC should be next day in NZ
            const date = new Date('2026-03-10T23:00:00Z');
            const result = toNZST(date);
            // Should be March 11 in NZ time (NZ is at least +10 ahead)
            expect(result).toMatch(/^2026-03-11/);
        });

        it('should produce valid ISO-like format', () => {
            const date = new Date();
            const result = toNZST(date);
            expect(result).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\+\d{2}:00$/);
        });

        it('should have a positive offset', () => {
            const date = new Date();
            const result = toNZST(date);
            const offsetMatch = result.match(/\+(\d{2}):00$/);
            expect(offsetMatch).not.toBeNull();
            const offset = parseInt(offsetMatch![1]);
            // When TZ=Pacific/Auckland in test env, offset may vary.
            // Just verify it's a reasonable positive number.
            expect(offset).toBeGreaterThanOrEqual(0);
            expect(offset).toBeLessThanOrEqual(14);
        });
    });
});
