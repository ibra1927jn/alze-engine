import { describe, it, expect } from 'vitest';
import {
    getBlockStatusColor,
    getBlockStatusBorder,
    getBlockTextColor,
    getStatusLabel,
    getRowProgress,
    getRowGradient,
    getVarietyStyle,
    AVATAR_COLORS,
} from './orchardMapUtils';

describe('orchardMapUtils — Color engine and status utilities', () => {
    describe('getBlockStatusColor', () => {
        it('returns gradient for idle', () => {
            expect(getBlockStatusColor('idle')).toContain('#f1f5f9');
        });
        it('returns gradient for active', () => {
            expect(getBlockStatusColor('active')).toContain('#fbbf24');
        });
        it('returns gradient for complete', () => {
            expect(getBlockStatusColor('complete')).toContain('#10b981');
        });
        it('returns gradient for alert', () => {
            expect(getBlockStatusColor('alert')).toContain('#ef4444');
        });
    });

    describe('getBlockStatusBorder', () => {
        it.each([
            ['idle', '#cbd5e1'],
            ['active', '#f59e0b'],
            ['complete', '#10b981'],
            ['alert', '#ef4444'],
        ] as const)('returns %s border color', (status, expected) => {
            expect(getBlockStatusBorder(status)).toBe(expected);
        });
    });

    describe('getBlockTextColor', () => {
        it('returns white for complete and alert', () => {
            expect(getBlockTextColor('complete')).toBe('#ffffff');
            expect(getBlockTextColor('alert')).toBe('#ffffff');
        });
        it('returns dark for idle and active', () => {
            expect(getBlockTextColor('idle')).toBe('#1e293b');
            expect(getBlockTextColor('active')).toBe('#1e293b');
        });
    });

    describe('getStatusLabel', () => {
        it('returns correct label and icon for each status', () => {
            expect(getStatusLabel('idle')).toEqual({ label: 'Unassigned', icon: 'hourglass_empty' });
            expect(getStatusLabel('active')).toEqual({ label: 'In Progress', icon: 'trending_up' });
            expect(getStatusLabel('complete')).toEqual({ label: 'Complete', icon: 'check_circle' });
            expect(getStatusLabel('alert')).toEqual({ label: 'Alert', icon: 'warning' });
        });
    });

    describe('getRowProgress', () => {
        it('returns 0 when target is 0', () => {
            expect(getRowProgress(50, 0)).toBe(0);
        });
        it('calculates correct progress ratio', () => {
            expect(getRowProgress(25, 100)).toBe(0.25);
            expect(getRowProgress(50, 100)).toBe(0.5);
        });
        it('caps at 1 (100%)', () => {
            expect(getRowProgress(150, 100)).toBe(1);
        });
        it('handles 0 buckets', () => {
            expect(getRowProgress(0, 100)).toBe(0);
        });
    });

    describe('getRowGradient', () => {
        it('returns grey for 0 progress', () => {
            expect(getRowGradient(0)).toContain('#f8fafc');
        });
        it('returns yellow for low progress', () => {
            expect(getRowGradient(0.1)).toContain('#fef9c3');
        });
        it('returns amber for moderate progress', () => {
            expect(getRowGradient(0.3)).toContain('#fbbf24');
        });
        it('returns green for good progress', () => {
            expect(getRowGradient(0.6)).toContain('#4ade80');
        });
        it('returns dark green for near-complete', () => {
            expect(getRowGradient(0.9)).toContain('#10b981');
        });
        it('returns deepest green for 100%', () => {
            expect(getRowGradient(1)).toContain('#059669');
        });
    });

    describe('getVarietyStyle', () => {
        it('returns correct style for known varieties', () => {
            const lapins = getVarietyStyle('Lapins');
            expect(lapins.bg).toBe('#fef2f2');
            expect(lapins.text).toBe('#991b1b');
            expect(lapins.dot).toBe('#dc2626');
        });

        it('returns fallback style for unknown variety', () => {
            const unknown = getVarietyStyle('UnknownCherry');
            expect(unknown.bg).toBe('#f8fafc');
            expect(unknown.text).toBe('#475569');
            expect(unknown.dot).toBe('#94a3b8');
        });

        it('covers all 6 known varieties', () => {
            const varieties = ['Lapins', 'Sweetheart', 'Bing', 'Rainier', 'Stella', 'Kordia'];
            for (const v of varieties) {
                const style = getVarietyStyle(v);
                expect(style.bg).toBeTruthy();
                expect(style.text).toBeTruthy();
                expect(style.dot).toBeTruthy();
            }
        });
    });

    describe('AVATAR_COLORS', () => {
        it('has 8 colors', () => {
            expect(AVATAR_COLORS).toHaveLength(8);
        });
        it('all are valid hex colors', () => {
            for (const color of AVATAR_COLORS) {
                expect(color).toMatch(/^#[0-9a-f]{6}$/);
            }
        });
    });
});
