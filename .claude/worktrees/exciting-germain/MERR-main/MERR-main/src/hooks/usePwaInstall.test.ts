/**
 * usePwaInstall Hook Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { usePwaInstall } from './usePwaInstall';

describe('usePwaInstall', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
        localStorage.clear();

        // Mock matchMedia for standalone detection
        vi.stubGlobal('matchMedia', vi.fn().mockImplementation(() => ({
            matches: false,
            addEventListener: vi.fn(),
            removeEventListener: vi.fn(),
        })));
    });

    it('initializes with canInstall=false', () => {
        const { result } = renderHook(() => usePwaInstall());
        expect(result.current.canInstall).toBe(false);
    });

    it('isInstalled is false when not standalone', () => {
        const { result } = renderHook(() => usePwaInstall());
        expect(result.current.isInstalled).toBe(false);
    });

    it('isDismissed is false initially', () => {
        const { result } = renderHook(() => usePwaInstall());
        expect(result.current.isDismissed).toBe(false);
    });

    it('promptInstall returns false without deferred prompt', async () => {
        const { result } = renderHook(() => usePwaInstall());
        let installed = false;
        await act(async () => { installed = await result.current.promptInstall(); });
        expect(installed).toBe(false);
    });

    it('dismissBanner sets isDismissed and persists to localStorage', () => {
        const { result } = renderHook(() => usePwaInstall());
        act(() => { result.current.dismissBanner(); });
        expect(result.current.isDismissed).toBe(true);
        expect(localStorage.getItem('pwa_install_dismissed')).toBeTruthy();
    });

    it('reads dismissed state from localStorage on init', () => {
        localStorage.setItem('pwa_install_dismissed', Date.now().toString());
        const { result } = renderHook(() => usePwaInstall());
        expect(result.current.isDismissed).toBe(true);
    });

    it('isDismissed expires after TTL', () => {
        // Set timestamp 8 days ago (beyond 7-day TTL)
        const eightDaysAgo = Date.now() - 8 * 24 * 60 * 60 * 1000;
        localStorage.setItem('pwa_install_dismissed', eightDaysAgo.toString());
        const { result } = renderHook(() => usePwaInstall());
        expect(result.current.isDismissed).toBe(false);
    });
});
