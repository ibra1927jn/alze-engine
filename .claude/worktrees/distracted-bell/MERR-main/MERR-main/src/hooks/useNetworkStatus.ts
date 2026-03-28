import { useState, useEffect, useCallback, useRef } from 'react';

interface NetworkStatus {
    /** Whether the browser is currently online */
    isOnline: boolean;
    /** Timestamp of the last time the device was online */
    lastOnline: number | null;
    /** True if the device was offline and just came back online (transition flag) */
    wasOffline: boolean;
    /** Clear the wasOffline flag after handling the reconnection */
    clearWasOffline: () => void;
}

/**
 * Centralized hook for online/offline detection.
 * 
 * Replaces duplicated `navigator.onLine` + event listener logic
 * found in SyncStatusMonitor, sync.service, and MessagingContext.
 * 
 * Usage:
 * ```tsx
 * const { isOnline, wasOffline, clearWasOffline } = useNetworkStatus();
 * 
 * useEffect(() => {
 *   if (wasOffline) {
 *     // Trigger sync, show toast, etc.
 *     clearWasOffline();
 *   }
 * }, [wasOffline, clearWasOffline]);
 * ```
 */
export function useNetworkStatus(): NetworkStatus {
    const [isOnline, setIsOnline] = useState(
        typeof navigator !== 'undefined' ? navigator.onLine : true
    );
    const [lastOnline, setLastOnline] = useState<number | null>(
        typeof navigator !== 'undefined' && navigator.onLine ? Date.now() : null
    );
    const [wasOffline, setWasOffline] = useState(false);
    const wasOfflineRef = useRef(false);

    const clearWasOffline = useCallback(() => {
        setWasOffline(false);
        wasOfflineRef.current = false;
    }, []);

    useEffect(() => {
        const handleOnline = () => {
            setIsOnline(true);
            setLastOnline(Date.now());
            // If we were offline before, set the transition flag
            if (wasOfflineRef.current) {
                setWasOffline(true);
            }
        };

        const handleOffline = () => {
            setIsOnline(false);
            wasOfflineRef.current = true;
        };

        window.addEventListener('online', handleOnline);
        window.addEventListener('offline', handleOffline);

        return () => {
            window.removeEventListener('online', handleOnline);
            window.removeEventListener('offline', handleOffline);
        };
    }, []);

    return { isOnline, lastOnline, wasOffline, clearWasOffline };
}
