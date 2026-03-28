/**
 * useMediaQuery Hook Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook } from '@testing-library/react';
import { useMediaQuery } from './useMediaQuery';

describe('useMediaQuery', () => {
    let matchMediaMock: ReturnType<typeof vi.fn>;

    beforeEach(() => {
        matchMediaMock = vi.fn().mockImplementation((query: string) => ({
            matches: query.includes('768') ? true : false,
            media: query,
            addEventListener: vi.fn(),
            removeEventListener: vi.fn(),
        }));
        vi.stubGlobal('matchMedia', matchMediaMock);
    });

    it('returns true when media query matches', () => {
        const { result } = renderHook(() => useMediaQuery('(min-width: 768px)'));
        expect(result.current).toBe(true);
    });

    it('returns false when media query does not match', () => {
        const { result } = renderHook(() => useMediaQuery('(min-width: 1200px)'));
        expect(result.current).toBe(false);
    });

    it('calls matchMedia with the provided query', () => {
        renderHook(() => useMediaQuery('(max-width: 500px)'));
        expect(matchMediaMock).toHaveBeenCalledWith('(max-width: 500px)');
    });
});
