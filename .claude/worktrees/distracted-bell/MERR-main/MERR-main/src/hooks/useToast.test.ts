/**
 * useToast Hook Tests
 */
import { describe, it, expect } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useToast } from './useToast';

describe('useToast', () => {
    it('initializes with null toast', () => {
        const { result } = renderHook(() => useToast());
        expect(result.current.toast).toBeNull();
    });

    it('showToast sets message and type', () => {
        const { result } = renderHook(() => useToast());
        act(() => { result.current.showToast('Saved!', 'success'); });
        expect(result.current.toast).toEqual({ message: 'Saved!', type: 'success' });
    });

    it('showToast defaults type to info', () => {
        const { result } = renderHook(() => useToast());
        act(() => { result.current.showToast('Hello'); });
        expect(result.current.toast?.type).toBe('info');
    });

    it('hideToast clears the toast', () => {
        const { result } = renderHook(() => useToast());
        act(() => { result.current.showToast('Test', 'error'); });
        expect(result.current.toast).not.toBeNull();
        act(() => { result.current.hideToast(); });
        expect(result.current.toast).toBeNull();
    });

    it('showToast replaces previous toast', () => {
        const { result } = renderHook(() => useToast());
        act(() => { result.current.showToast('First', 'info'); });
        act(() => { result.current.showToast('Second', 'warning'); });
        expect(result.current.toast).toEqual({ message: 'Second', type: 'warning' });
    });
});
