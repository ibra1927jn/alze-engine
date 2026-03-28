import { logger } from '@/utils/logger';
import React, { useState, useEffect, useCallback } from 'react';
import { useHarvestStore } from '../../stores/useHarvestStore';
import { offlineService } from '../../services/offline.service';
import { syncService } from '../../services/sync.service';
import { useNetworkStatus } from '../../hooks/useNetworkStatus';

const SyncStatusMonitor: React.FC = () => {
    const buckets = useHarvestStore((state) => state.buckets);
    const storePending = buckets.filter(b => !b.synced).length;
    const [vaultPending, setVaultPending] = useState<number>(0);
    const [lastSyncAgo, setLastSyncAgo] = useState<string | null>(null);
    const [maxRetries, setMaxRetries] = useState(0);
    const [showSuccess, setShowSuccess] = useState(false);
    const [isSyncing, setIsSyncing] = useState(false);

    const { isOnline, wasOffline, clearWasOffline } = useNetworkStatus();

    // Handle reconnection from offline → online (with jitter to prevent thundering herd)
    useEffect(() => {
        if (wasOffline && isOnline) {
            // Stagger sync by 0-30s to prevent hundreds of devices
            // from DDoS-ing Supabase when Wi-Fi returns at end of shift
            const delay = Math.floor(Math.random() * 30_000);
            const timer = setTimeout(() => syncService.processQueue(), delay);
            clearWasOffline();
            return () => clearTimeout(timer);
        }
    }, [wasOffline, isOnline, clearWasOffline]);

    // Poll Dexie for "True" pending count + sync metadata
    useEffect(() => {
        const checkStatus = async () => {
            try {
                const count = await offlineService.getPendingCount();
                setVaultPending(count);

                // Update retry count from sync queue (now async — Dexie)
                setMaxRetries(await syncService.getMaxRetryCount());

                // Update "last synced" relative time (now async — Dexie)
                const lastSync = await syncService.getLastSyncTime();
                if (lastSync) {
                    const diffMs = Date.now() - lastSync;
                    const diffSec = Math.floor(diffMs / 1000);
                    if (diffSec < 60) {
                        setLastSyncAgo('just now');
                    } else if (diffSec < 3600) {
                        setLastSyncAgo(`${Math.floor(diffSec / 60)}m ago`);
                    } else {
                        setLastSyncAgo(`${Math.floor(diffSec / 3600)}h ago`);
                    }
                }
            } catch (error) {
                logger.error('Failed to check sync status:', error);
            }
        };

        checkStatus();
        const interval = setInterval(checkStatus, 5000);
        return () => clearInterval(interval);
    }, []);

    // Show success toast when items finish syncing
    const displayCount = Math.max(storePending, vaultPending);
    const prevCountRef = React.useRef(displayCount);

    useEffect(() => {
        if (prevCountRef.current > 0 && displayCount === 0 && isOnline) {
            setShowSuccess(true);
            const timer = setTimeout(() => setShowSuccess(false), 3000);
            return () => clearTimeout(timer);
        }
        prevCountRef.current = displayCount;
    }, [displayCount, isOnline]);

    // Manual sync trigger
    const handleManualSync = useCallback(async () => {
        if (!isOnline || isSyncing) return;
        setIsSyncing(true);
        try {
            await syncService.processQueue();
        } finally {
            setIsSyncing(false);
        }
    }, [isOnline, isSyncing]);

    // 1. Offline Warning (High Priority)
    if (!isOnline) {
        return (
            <div className="bg-red-50 border-b border-red-100 px-4 py-2 flex items-center justify-between shrink-0 z-50 animate-in slide-in-from-top">
                <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-red-600 text-lg">wifi_off</span>
                    <p className="text-red-800 text-xs font-bold uppercase tracking-wide">You are Offline</p>
                </div>
                <div className="flex items-center gap-2">
                    {displayCount > 0 && (
                        <span className="text-[10px] font-bold bg-white/50 px-2 py-0.5 rounded text-red-700">
                            {displayCount} queued
                        </span>
                    )}
                    {lastSyncAgo && (
                        <span className="text-[10px] text-red-500">
                            Last sync: {lastSyncAgo}
                        </span>
                    )}
                </div>
            </div>
        );
    }

    // 2. Success Toast (auto-dismiss)
    if (showSuccess) {
        return (
            <div className="bg-green-50 border-b border-green-100 px-4 py-2 flex items-center justify-between shrink-0 z-50 animate-in slide-in-from-top">
                <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-green-600 text-lg">check_circle</span>
                    <p className="text-green-800 text-xs font-bold uppercase tracking-wide">All Synced</p>
                </div>
                {lastSyncAgo && (
                    <span className="text-[10px] text-green-600">
                        {lastSyncAgo}
                    </span>
                )}
            </div>
        );
    }

    // 3. Syncing State / Pending Queue
    if (displayCount > 0) {
        return (
            <div className="bg-orange-50 border-b border-orange-100 px-4 py-2 flex items-center justify-between shrink-0 z-50 animate-in slide-in-from-top">
                <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-orange-600 text-lg">cloud_sync</span>
                    <p className="text-orange-800 text-xs font-bold uppercase tracking-wide">
                        Syncing {displayCount} items...
                        {maxRetries > 0 && (
                            <span className="text-[10px] opacity-75 ml-1">(retry {maxRetries}/50)</span>
                        )}
                    </p>
                </div>

                <div className="flex items-center gap-3">
                    <button
                        onClick={handleManualSync}
                        disabled={isSyncing}
                        className="text-[10px] font-bold bg-orange-200 hover:bg-orange-300 px-2 py-0.5 rounded text-orange-800 transition-colors disabled:opacity-50"
                    >
                        {isSyncing ? 'Syncing...' : 'Sync Now'}
                    </button>
                    <div className="size-4 border-2 border-orange-200 border-t-orange-600 rounded-full animate-spin"></div>
                </div>
            </div>
        );
    }

    return null;
};

export default SyncStatusMonitor;