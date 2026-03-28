import posthog from 'posthog-js';
import { nowNZST } from '@/utils/nzst';
import { logger } from '@/utils/logger';

/**
 * Initialize PostHog for product analytics
 * 
 * FREE TIER: 1 million events/month (más que suficiente!)
 * 
 * Features incluidas:
 * - Event tracking
 * - User funnels
 * - Session recording
 * - Feature flags
 * - Dashboards
 */

export function initPostHog() {
    // Only initialize in staging and production
    if (import.meta.env.MODE === 'development') {
        logger.info('📊 PostHog disabled in development mode');
        return;
    }

    const apiKey = import.meta.env.VITE_POSTHOG_KEY;
    const host = import.meta.env.VITE_POSTHOG_HOST || 'https://app.posthog.com';

    if (!apiKey) {

        logger.warn('⚠️ VITE_POSTHOG_KEY not configured. Analytics will not track events.');
        return;
    }

    posthog.init(apiKey, {
        api_host: host,

        // Privacy settings
        autocapture: false, // Manual tracking only for privacy
        capture_pageview: true, // Track page views
        capture_pageleave: true, // Track when users leave

        // Session recording (opcional - comentado por defecto para privacidad)
        // session_recording: {
        //     maskAllInputs: true,
        //     maskTextSelector: '*',
        // },

        // Performance
        loaded: (posthog) => {
            if (import.meta.env.MODE === 'development') {
                posthog.opt_out_capturing(); // Disable in dev
            }
        },
    });
    logger.info('✅ PostHog initialized:', import.meta.env.MODE);
}

/**
 * Analytics service - Centralized event tracking
 */
export const analytics = {
    /**
     * Identify user (set user properties)
     */
    identify(userId: string, properties?: Record<string, unknown>) {
        posthog.identify(userId, properties);
    },

    /**
     * Track bucket scan
     */
    trackBucketScanned(pickerId: string, qualityGrade: string) {
        posthog.capture('bucket_scanned', {
            picker_id: pickerId,
            quality_grade: qualityGrade,
            timestamp: nowNZST(),
        });
    },

    /**
     * Track user login
     */
    trackLogin(role: string, orchardId?: string) {
        posthog.capture('user_login', {
            role,
            orchard_id: orchardId,
        });
    },

    /**
     * Track logout
     */
    trackLogout() {
        posthog.capture('user_logout');
        posthog.reset(); // Clear user identity
    },

    /**
     * Track picker check-in
     */
    trackCheckIn(pickerId: string) {
        posthog.capture('picker_check_in', {
            picker_id: pickerId,
            timestamp: nowNZST(),
        });
    },

    /**
     * Track offline sync
     */
    trackSync(itemCount: number, duration: number, success: boolean) {
        posthog.capture('offline_sync', {
            item_count: itemCount,
            duration_ms: duration,
            success,
        });
    },

    /**
     * Track broadcast sent
     */
    trackBroadcast(recipientCount: number, priority: string) {
        posthog.capture('broadcast_sent', {
            recipient_count: recipientCount,
            priority,
        });
    },

    /**
     * Track DLQ error
     */
    trackDLQError(errorType: string, severity: string) {
        posthog.capture('dlq_error', {
            error_type: errorType,
            severity,
        });
    },

    /**
     * Track feature usage
     */
    trackFeature(featureName: string, properties?: Record<string, unknown>) {
        posthog.capture(`feature_used:${featureName}`, properties);
    },

    /**
     * Track page view manually (if needed)
     */
    trackPageView(pageName: string) {
        posthog.capture('$pageview', {
            page: pageName,
        });
    },

    /**
     * Set user properties (for segmentation)
     */
    setUserProperties(properties: Record<string, unknown>) {
        posthog.people.set(properties);
    },
};

// Export posthog instance for advanced usage
export { posthog };