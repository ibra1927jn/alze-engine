/**
 * useLoginAnimations Hook Tests
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useTypewriter, useCounter, useParallax } from './useLoginAnimations';

describe('useTypewriter', () => {
    beforeEach(() => { vi.useFakeTimers(); });
    afterEach(() => { vi.useRealTimers(); });

    it('starts with empty string', () => {
        const { result } = renderHook(() => useTypewriter('Hello', 60, 0));
        expect(result.current.displayed).toBe('');
        expect(result.current.done).toBe(false);
    });

    it('reveals full text over time', () => {
        const { result } = renderHook(() => useTypewriter('Hi', 10, 0));
        // Advance past delay + enough time for all characters
        act(() => { vi.advanceTimersByTime(100); });
        expect(result.current.displayed).toBe('Hi');
        expect(result.current.done).toBe(true);
    });
});

describe('useCounter', () => {
    beforeEach(() => { vi.useFakeTimers(); });
    afterEach(() => { vi.useRealTimers(); });

    it('starts at 0', () => {
        const { result } = renderHook(() => useCounter(100, 1000, 0));
        expect(result.current).toBe(0);
    });
});

describe('useParallax', () => {
    it('returns ref and offset', () => {
        const { result } = renderHook(() => useParallax());
        expect(result.current.ref).toBeDefined();
        expect(result.current.offset).toEqual({ x: 0, y: 0 });
    });
});
