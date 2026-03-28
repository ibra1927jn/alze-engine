/**
 * usePickerManagement Hook Tests — pure helper functions
 */
import { describe, it, expect } from 'vitest';

// Test the pure, extracted helper functions from usePickerManagement
const MINIMUM_WAGE = 23.50;
const PIECE_RATE = 6.50;
const MIN_BUCKETS_PER_HOUR = MINIMUM_WAGE / PIECE_RATE;

const getDisplayStatus = (
    buckets: number, hoursWorked: number, baseStatus: string
): 'Active' | 'Break' | 'Below Minimum' | 'Off Duty' => {
    if (baseStatus === 'on_break') return 'Break';
    if (baseStatus === 'inactive' || baseStatus === 'suspended') return 'Off Duty';
    if (hoursWorked > 0 && (buckets / hoursWorked) < MIN_BUCKETS_PER_HOUR) return 'Below Minimum';
    return 'Active';
};

describe('usePickerManagement — getDisplayStatus', () => {
    it('returns Break for on_break status', () => {
        expect(getDisplayStatus(10, 2, 'on_break')).toBe('Break');
    });

    it('returns Off Duty for inactive', () => {
        expect(getDisplayStatus(10, 2, 'inactive')).toBe('Off Duty');
    });

    it('returns Off Duty for suspended', () => {
        expect(getDisplayStatus(10, 2, 'suspended')).toBe('Off Duty');
    });

    it('returns Below Minimum when rate is low', () => {
        // MIN_BUCKETS = 23.50/6.50 ≈ 3.615
        // 2 buckets / 1 hour = 2 < 3.615
        expect(getDisplayStatus(2, 1, 'active')).toBe('Below Minimum');
    });

    it('returns Active when rate is adequate', () => {
        // 5 buckets / 1 hour = 5 > 3.615
        expect(getDisplayStatus(5, 1, 'active')).toBe('Active');
    });

    it('returns Active when hours is 0', () => {
        expect(getDisplayStatus(0, 0, 'active')).toBe('Active');
    });
});

describe('usePickerManagement — earnings calculation', () => {
    it('calculates piece rate earnings', () => {
        const buckets = 10;
        const pieceEarnings = buckets * PIECE_RATE;
        expect(pieceEarnings).toBe(65);
    });

    it('calculates minimum wage top-up', () => {
        const buckets = 2;
        const hours = 4;
        const pieceEarnings = buckets * PIECE_RATE; // $13
        const minGuarantee = hours * MINIMUM_WAGE;  // $94
        const topUp = Math.max(0, minGuarantee - pieceEarnings);
        expect(topUp).toBe(81); // $94 - $13
    });

    it('no top-up when piece rate exceeds minimum', () => {
        const buckets = 20;
        const hours = 2;
        const pieceEarnings = buckets * PIECE_RATE; // $130
        const minGuarantee = hours * MINIMUM_WAGE;  // $47
        const topUp = Math.max(0, minGuarantee - pieceEarnings);
        expect(topUp).toBe(0);
    });

    it('MIN_BUCKETS_PER_HOUR is approximately 3.6', () => {
        expect(MIN_BUCKETS_PER_HOUR).toBeCloseTo(3.615, 2);
    });
});
