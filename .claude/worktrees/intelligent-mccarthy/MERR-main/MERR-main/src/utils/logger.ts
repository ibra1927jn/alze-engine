/**
 * Lightweight logger utility for HarvestPro NZ
 * 
 * - Production: errors forwarded to Sentry for monitoring
 * - Development: passes through to console
 * 
 * Usage:
 *   import { logger } from '@/utils/logger';
 *   logger.info('Fetched data', { count: 42 });
 *   logger.warn('Slow query');
 *   logger.error('Failed to sync', error);
 */

const isProd = typeof import.meta !== 'undefined' && import.meta.env?.PROD;

/* eslint-disable no-console */
export const logger = {
    info: (...args: unknown[]) => {
        if (!isProd) console.log('[HarvestPro]', ...args);
    },
    warn: (...args: unknown[]) => {
        if (!isProd) console.warn('[HarvestPro]', ...args);
    },
    error: (...args: unknown[]) => {
        // Always log errors to console
        console.error('[HarvestPro]', ...args);
        // 🔧 Forward errors to Sentry in production
        if (isProd) {
            try {
                // Lazy import to avoid circular dependencies during module init
                import('../config/sentry').then(({ captureSentryError }) => {
                    const errorArg = args.find(a => a instanceof Error);
                    if (errorArg) {
                        captureSentryError(errorArg as Error, { message: String(args[0]) });
                    } else {
                        captureSentryError(new Error(args.map(String).join(' ')));
                    }
                }).catch(() => { /* Sentry not available */ });
            } catch { /* Guard against import failures */ }
        }
    },
    debug: (...args: unknown[]) => {
        if (!isProd) console.debug('[HarvestPro]', ...args);
    },
};
/* eslint-enable no-console */

