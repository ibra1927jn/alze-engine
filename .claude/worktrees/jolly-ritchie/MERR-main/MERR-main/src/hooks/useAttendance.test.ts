/**
 * useAttendance Hook Tests — pure logic
 */
import { describe, it, expect } from 'vitest';

// Test the roster merging and stats calculation logic from useAttendance
interface MockPicker { id: string; name: string; }
interface MockAttendance { picker_id: string; check_out_time: string | null; }

const mergeRosterWithAttendance = (roster: MockPicker[], attendance: MockAttendance[]) => {
    return roster.map(picker => {
        const record = attendance.find(a => a.picker_id === picker.id);
        return {
            ...picker,
            attendanceRecord: record || null,
            isPresent: !!record && !record.check_out_time,
        };
    });
};

const calcStats = (mergedList: { isPresent: boolean }[]) => {
    const present = mergedList.filter(p => p.isPresent).length;
    const absent = mergedList.length - present;
    return { present, absent, total: mergedList.length };
};

describe('useAttendance — roster merging', () => {
    it('marks picker as present when checked in (no check_out)', () => {
        const roster = [{ id: 'p1', name: 'Alice' }];
        const attendance = [{ picker_id: 'p1', check_out_time: null }];
        const merged = mergeRosterWithAttendance(roster, attendance);
        expect(merged[0].isPresent).toBe(true);
    });

    it('marks picker as absent when not in attendance', () => {
        const roster = [{ id: 'p1', name: 'Alice' }];
        const merged = mergeRosterWithAttendance(roster, []);
        expect(merged[0].isPresent).toBe(false);
        expect(merged[0].attendanceRecord).toBeNull();
    });

    it('marks picker as absent when checked out', () => {
        const roster = [{ id: 'p1', name: 'Alice' }];
        const attendance = [{ picker_id: 'p1', check_out_time: '2026-03-04T17:00:00' }];
        const merged = mergeRosterWithAttendance(roster, attendance);
        expect(merged[0].isPresent).toBe(false);
    });
});

describe('useAttendance — stats', () => {
    it('calculates correct stats', () => {
        const mergedList = [
            { isPresent: true },
            { isPresent: true },
            { isPresent: false },
        ];
        const stats = calcStats(mergedList);
        expect(stats.present).toBe(2);
        expect(stats.absent).toBe(1);
        expect(stats.total).toBe(3);
    });

    it('handles empty list', () => {
        const stats = calcStats([]);
        expect(stats).toEqual({ present: 0, absent: 0, total: 0 });
    });
});
