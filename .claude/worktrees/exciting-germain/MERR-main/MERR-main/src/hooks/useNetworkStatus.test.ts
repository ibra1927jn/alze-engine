/**
 * useNetworkStatus Hook Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useNetworkStatus } from './useNetworkStatus';

describe('useNetworkStatus', () => {
    let addSpy: ReturnType<typeof vi.spyOn>;
    let removeSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        vi.restoreAllMocks();
        Object.defineProperty(navigator, 'onLine', { value: true, writable: true, configurable: true });
        addSpy = vi.spyOn(window, 'addEventListener');
        removeSpy = vi.spyOn(window, 'removeEventListener');
    });

    it('returns online status initially', () => {
        const { result } = renderHook(() => useNetworkStatus());
        expect(result.current.isOnline).toBe(true);
        expect(result.current.wasOffline).toBe(false);
    });

    it('registers online and offline event listeners', () => {
        renderHook(() => useNetworkStatus());
        expect(addSpy).toHaveBeenCalledWith('online', expect.any(Function));
        expect(addSpy).toHaveBeenCalledWith('offline', expect.any(Function));
    });

    it('removes listeners on unmount', () => {
        const { unmount } = renderHook(() => useNetworkStatus());
        unmount();
        expect(removeSpy).toHaveBeenCalledWith('online', expect.any(Function));
        expect(removeSpy).toHaveBeenCalledWith('offline', expect.any(Function));
    });

    it('transitions offline then back online sets wasOffline', () => {
        const { result } = renderHook(() => useNetworkStatus());

        // Simulate going offline
        const offlineHandler = addSpy.mock.calls.find(c => c[0] === 'offline')?.[1] as () => void;
        act(() => { offlineHandler(); });
        expect(result.current.isOnline).toBe(false);

        // Simulate going back online
        const onlineHandler = addSpy.mock.calls.find(c => c[0] === 'online')?.[1] as () => void;
        act(() => { onlineHandler(); });
        expect(result.current.isOnline).toBe(true);
        expect(result.current.wasOffline).toBe(true);
    });

    it('clearWasOffline resets the flag', () => {
        const { result } = renderHook(() => useNetworkStatus());

        // Trigger offline → online transition
        const offlineHandler = addSpy.mock.calls.find(c => c[0] === 'offline')?.[1] as () => void;
        const onlineHandler = addSpy.mock.calls.find(c => c[0] === 'online')?.[1] as () => void;
        act(() => { offlineHandler(); });
        act(() => { onlineHandler(); });
        expect(result.current.wasOffline).toBe(true);

        act(() => { result.current.clearWasOffline(); });
        expect(result.current.wasOffline).toBe(false);
    });
});
