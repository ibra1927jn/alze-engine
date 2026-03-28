/**
 * usePwaInstall — Hook to manage PWA install prompt
 * 
 * Captures the `beforeinstallprompt` event and provides a method
 * to trigger the native install dialog. Also tracks install state
 * to avoid showing the banner repeatedly.
 */
import { useState, useEffect, useCallback, useRef } from 'react';
import { logger } from '@/utils/logger';

interface BeforeInstallPromptEvent extends Event {
    prompt: () => Promise<void>;
    userChoice: Promise<{ outcome: 'accepted' | 'dismissed' }>;
}

interface PwaInstallState {
    /** Whether the native install prompt is available */
    canInstall: boolean;
    /** Whether the app is already installed (standalone) */
    isInstalled: boolean;
    /** Whether the user has dismissed the prompt before */
    isDismissed: boolean;
    /** Trigger the native install dialog */
    promptInstall: () => Promise<boolean>;
    /** Dismiss the install banner (persists to localStorage) */
    dismissBanner: () => void;
}

const DISMISS_KEY = 'pwa_install_dismissed';
const DISMISS_TTL = 7 * 24 * 60 * 60 * 1000; // Show again after 7 days

export function usePwaInstall(): PwaInstallState {
    const [canInstall, setCanInstall] = useState(false);
    const [isDismissed, setIsDismissed] = useState(() => {
        const ts = localStorage.getItem(DISMISS_KEY);
        if (!ts) return false;
        return Date.now() - parseInt(ts) < DISMISS_TTL;
    });

    const deferredPrompt = useRef<BeforeInstallPromptEvent | null>(null);

    const isInstalled =
        typeof window !== 'undefined' &&
        (window.matchMedia('(display-mode: standalone)').matches ||
            (window.navigator as unknown as { standalone?: boolean }).standalone === true);

    useEffect(() => {
        const handler = (e: Event) => {
            e.preventDefault();
            deferredPrompt.current = e as BeforeInstallPromptEvent;
            setCanInstall(true);
            logger.info('[PWA] Install prompt captured');
        };

        window.addEventListener('beforeinstallprompt', handler);

        // Track when app is actually installed
        window.addEventListener('appinstalled', () => {
            setCanInstall(false);
            deferredPrompt.current = null;
            logger.info('[PWA] App installed successfully');
        });

        return () => window.removeEventListener('beforeinstallprompt', handler);
    }, []);

    const promptInstall = useCallback(async (): Promise<boolean> => {
        if (!deferredPrompt.current) return false;

        await deferredPrompt.current.prompt();
        const { outcome } = await deferredPrompt.current.userChoice;

        if (outcome === 'accepted') {
            logger.info('[PWA] User accepted install');
            deferredPrompt.current = null;
            setCanInstall(false);
            return true;
        } else {
            logger.info('[PWA] User dismissed install');
            return false;
        }
    }, []);

    const dismissBanner = useCallback(() => {
        localStorage.setItem(DISMISS_KEY, Date.now().toString());
        setIsDismissed(true);
    }, []);

    return { canInstall, isInstalled, isDismissed, promptInstall, dismissBanner };
}
