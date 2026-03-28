/**
 * useScanRateLimit Hook Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useScanRateLimit } from './useScanRateLimit';

// Mock navigator.vibrate
Object.defineProperty(navigator, 'vibrate', { value: vi.fn(), writable: true });

describe('useScanRateLimit', () => {
    let onScan: ReturnType<typeof vi.fn>;

    beforeEach(() => {
        vi.clearAllMocks();
        vi.useFakeTimers();
        onScan = vi.fn();
    });

    afterEach(() => { vi.useRealTimers(); });

    it('accepts first scan immediately', () => {
        const { result } = renderHook(() => useScanRateLimit(onScan));
        act(() => { result.current.handleScan('CODE-001'); });
        expect(onScan).toHaveBeenCalledWith('CODE-001');
    });

    it('blocks same code within cooldown', () => {
        const { result } = renderHook(() => useScanRateLimit(onScan, { sameScanCooldownMs: 3000 }));
        act(() => { result.current.handleScan('CODE-001'); });
        expect(onScan).toHaveBeenCalledTimes(1);

        // Same code within 3s → blocked
        act(() => { result.current.handleScan('CODE-001'); });
        expect(onScan).toHaveBeenCalledTimes(1);
    });

    it('allows same code after cooldown expires', () => {
        const { result } = renderHook(() => useScanRateLimit(onScan, { sameScanCooldownMs: 1000 }));
        act(() => { result.current.handleScan('CODE-001'); });
        expect(onScan).toHaveBeenCalledTimes(1);

        // Advance past cooldown
        act(() => { vi.advanceTimersByTime(1500); });
        act(() => { result.current.handleScan('CODE-001'); });
        expect(onScan).toHaveBeenCalledTimes(2);
    });

    it('allows different code after global debounce', () => {
        const { result } = renderHook(() =>
            useScanRateLimit(onScan, { globalDebounceMs: 500 })
        );
        act(() => { result.current.handleScan('CODE-001'); });
        expect(onScan).toHaveBeenCalledTimes(1);

        // Different code immediately → debounced
        act(() => { result.current.handleScan('CODE-002'); });
        // Should fire after debounce period
        act(() => { vi.advanceTimersByTime(600); });
        expect(onScan).toHaveBeenCalledWith('CODE-002');
    });

    it('vibrates on success', () => {
        const { result } = renderHook(() => useScanRateLimit(onScan));
        act(() => { result.current.handleScan('CODE-001'); });
        expect(navigator.vibrate).toHaveBeenCalledWith([200]);
    });

    it('vibrates differently on rejection', () => {
        const { result } = renderHook(() => useScanRateLimit(onScan, { sameScanCooldownMs: 3000 }));
        act(() => { result.current.handleScan('CODE-001'); });
        act(() => { result.current.handleScan('CODE-001'); }); // Duplicate
        expect(navigator.vibrate).toHaveBeenCalledWith([50, 50, 50]);
    });
});
