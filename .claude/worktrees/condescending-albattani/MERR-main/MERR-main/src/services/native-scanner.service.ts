/**
 * Native Scanner Service — Capacitor Bridge for QR/Barcode Scanning
 *
 * Strategy:
 * 1. Check if running in native Capacitor shell → use native BarcodeScanner
 * 2. Fall back to html5-qrcode for PWA/browser mode
 *
 * Native scanning benefits:
 * - 10-50× faster than web camera API
 * - Uses hardware accelerated ML (Vision framework on iOS, ML Kit on Android)
 * - Works in bright sunlight (adaptive exposure)
 * - Lower battery consumption
 */

import { logger } from '@/utils/logger';

export type ScanResult = {
    code: string;
    format: string;
    source: 'native' | 'web';
};

export type ScannerPlatform = 'native' | 'web' | 'unknown';

/**
 * Detect if running inside a Capacitor native shell
 */
export function getPlatform(): ScannerPlatform {
    // Capacitor injects this global when running in a native shell
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const win = window as any;
    if (win?.Capacitor?.isNativePlatform?.()) {
        return 'native';
    }
    if (win?.Capacitor?.getPlatform?.() === 'web') {
        return 'web';
    }
    return 'web';
}

export function isNativePlatform(): boolean {
    return getPlatform() === 'native';
}

/**
 * Request native barcode scan using Capacitor BarcodeScanner plugin.
 * Returns null if not available or user cancels.
 */
export async function scanNative(): Promise<ScanResult | null> {
    try {
        // Dynamic import — only loaded on native platform
        const { BarcodeScanner } = await import('@capacitor-community/barcode-scanner');

        // Check/request camera permission
        const status = await BarcodeScanner.checkPermission({ force: true });
        if (!status.granted) {
            logger.warn('[NativeScanner] Camera permission denied');
            return null;
        }

        // Make background transparent for camera preview
        document.body.classList.add('scanner-active');
        BarcodeScanner.hideBackground();

        const result = await BarcodeScanner.startScan();

        // Restore background
        document.body.classList.remove('scanner-active');
        BarcodeScanner.showBackground();

        if (result.hasContent && result.content) {
            return {
                code: result.content,
                format: result.format || 'QR_CODE',
                source: 'native',
            };
        }

        return null;
    } catch (error) {
        document.body.classList.remove('scanner-active');
        logger.error('[NativeScanner] Error:', error);
        return null;
    }
}

/**
 * Stop native scanner (e.g. when user closes modal)
 */
export async function stopNativeScan(): Promise<void> {
    try {
        if (isNativePlatform()) {
            const { BarcodeScanner } = await import('@capacitor-community/barcode-scanner');
            await BarcodeScanner.stopScan();
            BarcodeScanner.showBackground();
            document.body.classList.remove('scanner-active');
        }
    } catch {
        // Ignore — scanner may not be running
    }
}

export const nativeScannerService = {
    getPlatform,
    isNativePlatform,
    scanNative,
    stopNativeScan,
};
