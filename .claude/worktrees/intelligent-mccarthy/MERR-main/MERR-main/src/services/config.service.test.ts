/**
 * config.service.test.ts — Unit tests for configuration service
 * Uses vi.stubEnv() for import.meta.env (read-only in Vitest)
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

// Must reset modules between tests since loadConfig() is a singleton
let getConfig: typeof import('./config.service').getConfig;
let resetConfig: typeof import('./config.service').resetConfig;
let isFeatureEnabled: typeof import('./config.service').isFeatureEnabled;
let getLogLevel: typeof import('./config.service').getLogLevel;
let ConfigurationError: typeof import('./config.service').ConfigurationError;

async function reimport() {
    vi.resetModules();
    const mod = await import('./config.service');
    getConfig = mod.getConfig;
    resetConfig = mod.resetConfig;
    isFeatureEnabled = mod.isFeatureEnabled;
    getLogLevel = mod.getLogLevel;
    ConfigurationError = mod.ConfigurationError;
}

beforeEach(async () => {
    vi.unstubAllEnvs();
    vi.stubEnv('VITE_SUPABASE_URL', 'https://test.supabase.co');
    vi.stubEnv('VITE_SUPABASE_ANON_KEY', 'test-anon-key');
    vi.stubEnv('MODE', 'development');
    await reimport();
});

afterEach(() => {
    resetConfig?.();
    vi.unstubAllEnvs();
});

describe('config.service', () => {
    describe('getConfig', () => {
        it('returns config with required fields', () => {
            const config = getConfig();
            expect(config.SUPABASE_URL).toBe('https://test.supabase.co');
            expect(config.SUPABASE_ANON_KEY).toBe('test-anon-key');
        });

        it('returns singleton (same object on second call)', () => {
            const c1 = getConfig();
            const c2 = getConfig();
            expect(c1).toBe(c2);
        });

        it('detects development environment', () => {
            const config = getConfig();
            expect(config.isDevelopment).toBe(true);
            expect(config.isProduction).toBe(false);
            expect(config.environment).toBe('development');
        });

        it('includes optional config with defaults', () => {
            const config = getConfig();
            expect(config.APP_VERSION).toBeDefined();
            expect(config.LOG_LEVEL).toBe('info');
        });

        it('reads ENABLE_ANALYTICS from env', async () => {
            vi.stubEnv('VITE_ENABLE_ANALYTICS', 'true');
            await reimport();
            const config = getConfig();
            expect(config.ENABLE_ANALYTICS).toBe(true);
        });
    });

    describe('resetConfig', () => {
        it('clears singleton so next getConfig creates fresh config', () => {
            const c1 = getConfig();
            resetConfig();
            const c2 = getConfig();
            expect(c1).not.toBe(c2);
        });
    });

    describe('isFeatureEnabled', () => {
        it('returns true when feature env var is "true"', async () => {
            vi.stubEnv('VITE_FEATURE_DARK_MODE', 'true');
            await reimport();
            expect(isFeatureEnabled('dark_mode')).toBe(true);
        });

        it('returns true when feature env var is "1"', async () => {
            vi.stubEnv('VITE_FEATURE_BETA', '1');
            await reimport();
            expect(isFeatureEnabled('beta')).toBe(true);
        });

        it('returns false when feature env var is not set', () => {
            expect(isFeatureEnabled('nonexistent')).toBe(false);
        });

        it('returns false when feature env var is "false"', async () => {
            vi.stubEnv('VITE_FEATURE_TEST', 'false');
            await reimport();
            expect(isFeatureEnabled('test')).toBe(false);
        });
    });

    describe('getLogLevel', () => {
        it('returns "info" by default', () => {
            expect(getLogLevel()).toBe('info');
        });

        it('returns configured log level from env', async () => {
            vi.stubEnv('VITE_LOG_LEVEL', 'debug');
            await reimport();
            expect(getLogLevel()).toBe('debug');
        });
    });

    describe('ConfigurationError', () => {
        it('is an Error with correct name', () => {
            const err = new ConfigurationError('missing keys', ['KEY1']);
            expect(err).toBeInstanceOf(Error);
            expect(err.name).toBe('ConfigurationError');
            expect(err.message).toBe('missing keys');
            expect(err.missingKeys).toEqual(['KEY1']);
        });

        it('defaults missingKeys to empty array', () => {
            const err = new ConfigurationError('test');
            expect(err.missingKeys).toEqual([]);
        });
    });
});
