// =============================================
// COMPLIANCE SERVICE TESTS
// =============================================
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import * as nzstModule from '@/utils/nzst';
import {
    calculateNextBreakDue,
    isBreakOverdue,
    getRequiredBreakDuration,
    calculateEffectiveHourlyRate,
    checkWageCompliance,
    getMinimumBucketsPerHour,
    checkWorkHoursCompliance,
    checkPickerCompliance,
    NZ_BREAK_REQUIREMENTS,
} from './compliance.service';

describe('Compliance Service', () => {
    // Freeze time so that nowNZST() returns a predictable value
    const FIXED_TIME = new Date('2024-01-01T12:00:00').getTime();

    beforeEach(() => {
        vi.useFakeTimers();
        vi.setSystemTime(FIXED_TIME);
        vi.spyOn(nzstModule, 'nowNZST').mockReturnValue('2024-01-01T12:00:00');
    });

    afterEach(() => {
        vi.useRealTimers();
        vi.restoreAllMocks();
    });
    // =============================================
    // BREAK COMPLIANCE
    // =============================================
    describe('Break Compliance', () => {
        describe('calculateNextBreakDue', () => {
            it('should calculate rest break due 2 hours after last break', () => {
                const lastBreak = new Date('2024-01-01T08:00:00');
                const workStart = new Date('2024-01-01T07:00:00');
                const nextDue = calculateNextBreakDue(lastBreak, 'rest', workStart);

                expect(nextDue.getTime() - lastBreak.getTime()).toBe(
                    NZ_BREAK_REQUIREMENTS.REST_BREAK_INTERVAL_MINUTES * 60 * 1000
                );
            });

            it('should calculate meal break due 4 hours after work start if no previous break', () => {
                const workStart = new Date('2024-01-01T07:00:00');
                const nextDue = calculateNextBreakDue(null, 'meal', workStart);

                expect(nextDue.getTime() - workStart.getTime()).toBe(
                    NZ_BREAK_REQUIREMENTS.MEAL_BREAK_INTERVAL_MINUTES * 60 * 1000
                );
            });
        });

        describe('isBreakOverdue', () => {
            it('should detect overdue rest break', () => {
                // Last break was 3 hours ago (well beyond 2hr interval)
                const now = FIXED_TIME;
                const lastBreak = new Date(now - 3 * 60 * 60 * 1000);
                const workStart = new Date(now - 4 * 60 * 60 * 1000);

                const result = isBreakOverdue(lastBreak, 'rest', workStart);
                expect(result.overdue).toBe(true);
                expect(result.minutesOverdue).toBeGreaterThan(0);
            });

            it('should not flag break as overdue if within time', () => {
                // Last break was 1 hour ago (within 2hr interval)
                const now = FIXED_TIME;
                const lastBreak = new Date(now - 1 * 60 * 60 * 1000);
                const workStart = new Date(now - 2 * 60 * 60 * 1000);

                const result = isBreakOverdue(lastBreak, 'rest', workStart);
                expect(result.overdue).toBe(false);
                expect(result.minutesOverdue).toBe(0);
            });
        });

        describe('getRequiredBreakDuration', () => {
            it('should return 10 minutes for rest breaks', () => {
                expect(getRequiredBreakDuration('rest')).toBe(10);
            });

            it('should return 30 minutes for meal breaks', () => {
                expect(getRequiredBreakDuration('meal')).toBe(30);
            });

            it('should return 5 minutes for hydration breaks', () => {
                expect(getRequiredBreakDuration('hydration')).toBe(5);
            });
        });
    });

    // =============================================
    // WAGE COMPLIANCE
    // =============================================
    describe('Wage Compliance', () => {
        describe('calculateEffectiveHourlyRate', () => {
            it('should calculate correct hourly rate', () => {
                // 10 buckets at $6.50 in 2 hours = $32.50/hr
                const rate = calculateEffectiveHourlyRate(10, 2, 6.5);
                expect(rate).toBe(32.5);
            });

            it('should return 0 for zero hours', () => {
                expect(calculateEffectiveHourlyRate(10, 0, 6.5)).toBe(0);
            });
        });

        describe('checkWageCompliance', () => {
            it('should be compliant when earning above minimum wage', () => {
                // 10 buckets at $6.50 in 2 hours = $32.50/hr > $23.50
                const result = checkWageCompliance(10, 2, 6.5);
                expect(result.isCompliant).toBe(true);
                expect(result.effectiveHourlyRate).toBe(32.5);
                expect(result.topUpRequired).toBe(0);
            });

            it('should be non-compliant when earning below minimum wage', () => {
                // 2 buckets at $6.50 in 2 hours = $6.50/hr < $23.50
                const result = checkWageCompliance(2, 2, 6.5);
                expect(result.isCompliant).toBe(false);
                expect(result.shortfall).toBeGreaterThan(0);
                expect(result.topUpRequired).toBeGreaterThan(0);
            });

            it('should calculate correct top-up amount', () => {
                // 4 buckets at $6.50 in 2 hours = $26 earned
                // Min wage (23.50) * 2 = $47.00
                // Top-up = $47.00 - $26 = $21.00
                const result = checkWageCompliance(4, 2, 6.5);
                expect(result.topUpRequired).toBe(21);
            });
        });

        describe('getMinimumBucketsPerHour', () => {
            it('should calculate minimum buckets to earn minimum wage', () => {
                // $23.50 / $6.50 = 3.615... ≈ 3.7 buckets/hr
                const minBuckets = getMinimumBucketsPerHour(6.5);
                expect(minBuckets).toBeGreaterThanOrEqual(3.6);
                expect(minBuckets).toBeLessThanOrEqual(4);
            });
        });
    });

    // =============================================
    // WORK HOURS COMPLIANCE
    // =============================================
    describe('Work Hours Compliance', () => {
        describe('checkWorkHoursCompliance', () => {
            it('should flag need for break after 110 minutes', () => {
                const result = checkWorkHoursCompliance(115, 200);
                expect(result.needsBreak).toBe(true);
            });

            it('should not flag need for break within first 110 minutes', () => {
                const result = checkWorkHoursCompliance(60, 60);
                expect(result.needsBreak).toBe(false);
            });

            it('should flag when recommended daily hours exceeded', () => {
                // 13 hours = 780 minutes
                const result = checkWorkHoursCompliance(60, 780);
                expect(result.maxRecommendedReached).toBe(true);
            });

            it('should provide warning for excessive consecutive work', () => {
                // 11 hours = 660 minutes
                const result = checkWorkHoursCompliance(660, 660);
                expect(result.warning).toBeDefined();
                expect(result.warning).toContain('11 hours');
            });
        });
    });

    // =============================================
    // FULL COMPLIANCE CHECK
    // =============================================
    describe('checkPickerCompliance', () => {
        it('should return compliant status for good picker', () => {
            const now = new Date(FIXED_TIME);
            const workStart = new Date(now.getTime() - 1 * 60 * 60 * 1000); // 1 hour ago
            const recentBreak = new Date(now.getTime() - 30 * 60 * 1000); // 30 min ago

            const result = checkPickerCompliance({
                pickerId: 'picker-1',
                bucketCount: 10,
                hoursWorked: 1,
                consecutiveMinutesWorked: 60,
                totalMinutesToday: 60,
                lastRestBreakAt: recentBreak,
                lastMealBreakAt: null,
                lastHydrationAt: recentBreak,
                workStartTime: workStart,
            });

            expect(result.isCompliant).toBe(true);
            expect(result.wageCompliance.isCompliant).toBe(true);
        });

        it('should detect wage violation', () => {
            const now = new Date();
            const workStart = new Date(now.getTime() - 2 * 60 * 60 * 1000);
            const recentBreak = new Date(now.getTime() - 30 * 60 * 1000);

            const result = checkPickerCompliance({
                pickerId: 'picker-1',
                bucketCount: 2, // Only 2 buckets in 2 hours = ~$6.50/hr
                hoursWorked: 2,
                consecutiveMinutesWorked: 30,
                totalMinutesToday: 120,
                lastRestBreakAt: recentBreak,
                lastMealBreakAt: null,
                lastHydrationAt: recentBreak,
                workStartTime: workStart,
            });

            expect(result.wageCompliance.isCompliant).toBe(false);
            expect(result.violations.some((v) => v.type === 'wage_below_minimum')).toBe(true);
        });

        it('should detect overdue rest break', () => {
            const now = new Date();
            const workStart = new Date(now.getTime() - 4 * 60 * 60 * 1000);
            // Last rest break was 3 hours ago
            const oldBreak = new Date(now.getTime() - 3 * 60 * 60 * 1000);

            const result = checkPickerCompliance({
                pickerId: 'picker-1',
                bucketCount: 20,
                hoursWorked: 4,
                consecutiveMinutesWorked: 180,
                totalMinutesToday: 240,
                lastRestBreakAt: oldBreak,
                lastMealBreakAt: workStart,
                lastHydrationAt: oldBreak,
                workStartTime: workStart,
            });

            expect(result.violations.some((v) => v.type === 'break_overdue')).toBe(true);
        });

        it('should include next break due information', () => {
            const now = new Date();
            const workStart = new Date(now.getTime() - 1 * 60 * 60 * 1000);

            const result = checkPickerCompliance({
                pickerId: 'picker-1',
                bucketCount: 10,
                hoursWorked: 1,
                consecutiveMinutesWorked: 60,
                totalMinutesToday: 60,
                lastRestBreakAt: null,
                lastMealBreakAt: null,
                lastHydrationAt: null,
                workStartTime: workStart,
            });

            expect(result.nextBreakDue).toBeDefined();
            expect(result.nextBreakDue?.type).toBeDefined();
            expect(result.nextBreakDue?.dueAt).toBeInstanceOf(Date);
        });
    });
});
