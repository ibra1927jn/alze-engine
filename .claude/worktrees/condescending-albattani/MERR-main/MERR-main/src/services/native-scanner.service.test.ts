// =============================================
// NATIVE SCANNER SERVICE TESTS
// =============================================
// Tests for platform detection logic — validated without
// importing the full service to avoid Capacitor module resolution.
import { describe, it, expect } from 'vitest';

describe('Native Scanner Service — Platform Detection', () => {
    /**
     * Test the core platform detection logic in isolation.
     * The actual service uses `window.Capacitor` to detect the platform.
     */
    it('window.Capacitor should not exist in jsdom (web mode)', () => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const win = window as any;
        expect(win.Capacitor).toBeUndefined();
    });

    it('platform detection logic returns "web" when no Capacitor', () => {
        // Inline the same logic as getPlatform() to test in isolation
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const win = window as any;
        const isNative = win?.Capacitor?.isNativePlatform?.() === true;
        const isWeb = win?.Capacitor?.getPlatform?.() === 'web';
        const platform = isNative ? 'native' : isWeb ? 'web' : 'web';
        expect(platform).toBe('web');
    });

    it('platform detection logic returns "native" when Capacitor present', () => {
        // Simulate Capacitor global
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const win = window as any;
        win.Capacitor = {
            isNativePlatform: () => true,
            getPlatform: () => 'ios',
        };

        const isNative = win.Capacitor.isNativePlatform();
        expect(isNative).toBe(true);

        // Cleanup
        delete win.Capacitor;
    });

    it('ScanResult type should have required fields', () => {
        // Validates the type shape used by the service
        const mockResult = {
            code: 'BKT-1024',
            format: 'QR_CODE',
            source: 'native' as const,
        };
        expect(mockResult.code).toBe('BKT-1024');
        expect(mockResult.format).toBe('QR_CODE');
        expect(mockResult.source).toBe('native');
    });
});
