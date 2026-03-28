/**
 * useScanRateLimit — Intelligent rate limiting for QR scanner
 * 
 * SMART LOGIC (per counterpart pro-tip):
 * - Same code scanned again → BLOCK for 3000ms (trembling hand duplicate)
 * - Different code scanned → Allow after small 500ms debounce (legitimate rapid scanning)
 * - Haptic feedback on success/rejection
 */
import { useCallback, useRef } from 'react';
import { logger } from '@/utils/logger';

interface ScanRateLimitOptions {
    /** Cooldown for SAME code re-scan (trembling hand protection). Default: 3000ms */
    sameScanCooldownMs?: number;
    /** Minimum gap between ANY scans (anti-spam). Default: 500ms */
    globalDebounceMs?: number;
}

interface ScanRateLimitResult {
    /** Wrap onScan with this — it applies the intelligent rate limiting */
    handleScan: (code: string) => void;
    /** True if currently in cooldown for same-code block */
    isCoolingDown: boolean;
    /** The last successfully accepted scan code */
    lastAcceptedCode: string | null;
}

export function useScanRateLimit(
    onScan: (code: string) => void,
    options: ScanRateLimitOptions = {},
): ScanRateLimitResult {
    const {
        sameScanCooldownMs = 3000,
        globalDebounceMs = 500,
    } = options;

    const lastScanTime = useRef<number>(0);
    const lastScanCode = useRef<string | null>(null);
    const isCoolingDownRef = useRef(false);

    const handleScan = useCallback((code: string) => {
        const now = Date.now();
        const timeSinceLastScan = now - lastScanTime.current;
        const isSameCode = lastScanCode.current === code;

        // Case 1: Same code scanned again within long cooldown → REJECT (trembling hand)
        if (isSameCode && timeSinceLastScan < sameScanCooldownMs) {
            logger.info(`[Scanner] Duplicate scan blocked: "${code}" (${timeSinceLastScan}ms ago)`);
            isCoolingDownRef.current = true;
            // Haptic rejection feedback: short triple vibration
            if (navigator.vibrate) {
                navigator.vibrate([50, 50, 50]);
            }
            return;
        }

        // Case 2: Different code but too fast → REJECT (mechanical spam)
        if (!isSameCode && timeSinceLastScan < globalDebounceMs) {
            logger.info(`[Scanner] Debounce: different code "${code}" too fast (${timeSinceLastScan}ms)`);
            // Still allow after debounce — schedule it
            const remaining = globalDebounceMs - timeSinceLastScan;
            setTimeout(() => {
                lastScanTime.current = Date.now();
                lastScanCode.current = code;
                isCoolingDownRef.current = false;
                // Haptic success
                if (navigator.vibrate) {
                    navigator.vibrate([200]);
                }
                onScan(code);
            }, remaining);
            return;
        }

        // Case 3: Legitimate scan → ACCEPT
        lastScanTime.current = now;
        lastScanCode.current = code;
        isCoolingDownRef.current = false;
        // Haptic success feedback
        if (navigator.vibrate) {
            navigator.vibrate([200]);
        }
        logger.info(`[Scanner] Scan accepted: "${code}"`);
        onScan(code);
    }, [onScan, sameScanCooldownMs, globalDebounceMs]);

    return {
        handleScan,
        isCoolingDown: isCoolingDownRef.current,
        lastAcceptedCode: lastScanCode.current,
    };
}
