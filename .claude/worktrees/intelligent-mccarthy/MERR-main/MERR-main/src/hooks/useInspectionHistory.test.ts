/**
 * useInspectionHistory Hook Tests
 */
import { describe, it, expect, vi, afterEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useInspectionHistory } from './useInspectionHistory';

// Mock NZST utils
vi.mock('@/utils/nzst', () => ({
    nowNZST: () => '2026-03-04T12:00:00+13:00',
    toNZST: (d: Date) => d.toISOString(),
}));

describe('useInspectionHistory', () => {
    afterEach(() => { vi.clearAllMocks(); });

    it('initializes with empty inspections', () => {
        const { result } = renderHook(() => useInspectionHistory());
        expect(result.current.inspections).toEqual([]);
        expect(result.current.isLoading).toBe(false);
    });

    it('stats are zero when empty', () => {
        const { result } = renderHook(() => useInspectionHistory());
        expect(result.current.stats).toEqual({
            total: 0, good: 0, warning: 0, bad: 0, averageScore: 0,
        });
    });

    it('loadInspections populates data', async () => {
        const { result } = renderHook(() => useInspectionHistory());
        await act(async () => { await result.current.loadInspections('picker-1'); });
        expect(result.current.inspections.length).toBeGreaterThan(0);
        expect(result.current.stats.total).toBeGreaterThan(0);
    });

    it('addInspection prepends to list', () => {
        const { result } = renderHook(() => useInspectionHistory());
        act(() => {
            result.current.addInspection({
                picker_id: 'p1', inspector_id: 'i1',
                quality_grade: 'A', notes: 'Good',
            });
        });
        expect(result.current.inspections).toHaveLength(1);
        expect(result.current.inspections[0].quality_grade).toBe('A');
    });

    it('calculates stats correctly', () => {
        const { result } = renderHook(() => useInspectionHistory());
        act(() => {
            result.current.addInspection({ picker_id: 'p1', inspector_id: 'i1', quality_grade: 'A', notes: '' });
            result.current.addInspection({ picker_id: 'p1', inspector_id: 'i1', quality_grade: 'B', notes: '' });
            result.current.addInspection({ picker_id: 'p1', inspector_id: 'i1', quality_grade: 'C', notes: '' });
        });
        expect(result.current.stats.total).toBe(3);
        expect(result.current.stats.good).toBe(1);
        expect(result.current.stats.warning).toBe(1);
        expect(result.current.stats.bad).toBe(1);
        expect(result.current.stats.averageScore).toBe(50); // (100 + 50 + 0) / 3
    });

    it('getGradeColor returns correct colors', () => {
        const { result } = renderHook(() => useInspectionHistory());
        expect(result.current.getGradeColor('A')).toBe('#22c55e');
        expect(result.current.getGradeColor('B')).toBe('#f59e0b');
        expect(result.current.getGradeColor('C')).toBe('#ef4444');
        expect(result.current.getGradeColor('reject')).toBe('#7f1d1d');
    });

    it('getGradeLabel returns labels', () => {
        const { result } = renderHook(() => useInspectionHistory());
        expect(result.current.getGradeLabel('A')).toBe('Grade A');
        expect(result.current.getGradeLabel('reject')).toBe('Rejected');
    });
});
