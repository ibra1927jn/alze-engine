/**
 * i18n.service.test.ts — Unit tests for internationalization service
 *
 * I18nService is a singleton. We use the real translation files
 * and test with actual translation keys from en.ts / es.ts / sm.ts / to.ts.
 * All 4 languages have the same set of keys, so fallback is only triggered
 * for keys that exist in no language (returns key itself).
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn(), debug: vi.fn() },
}));

import { i18n, t, SUPPORTED_LANGUAGES } from './i18n.service';

describe('I18nService', () => {
    beforeEach(() => {
        vi.clearAllMocks();
        localStorage.clear();
        i18n.setLanguage('en');
    });

    describe('SUPPORTED_LANGUAGES', () => {
        it('includes en, es, sm, to', () => {
            const codes = SUPPORTED_LANGUAGES.map(l => l.code);
            expect(codes).toEqual(['en', 'es', 'sm', 'to']);
        });

        it('each language has code, name, nativeName, flag', () => {
            SUPPORTED_LANGUAGES.forEach(lang => {
                expect(lang.code).toBeTruthy();
                expect(lang.name).toBeTruthy();
                expect(lang.nativeName).toBeTruthy();
                expect(lang.flag).toBeTruthy();
            });
        });
    });

    describe('getLanguage / setLanguage', () => {
        it('returns current language', () => {
            expect(i18n.getLanguage()).toBe('en');
        });

        it('changes language and saves to localStorage', () => {
            i18n.setLanguage('es');
            expect(i18n.getLanguage()).toBe('es');
            expect(localStorage.getItem('harvestpro_language')).toBe('es');
        });

        it('does nothing if setting same language', () => {
            const spy = vi.spyOn(Storage.prototype, 'setItem');
            i18n.setLanguage('en'); // already en
            expect(spy).not.toHaveBeenCalled();
            spy.mockRestore();
        });
    });

    describe('getLanguageInfo', () => {
        it('returns correct info for current language', () => {
            i18n.setLanguage('es');
            const info = i18n.getLanguageInfo();
            expect(info.code).toBe('es');
            expect(info.name).toBe('Spanish');
            expect(info.nativeName).toBe('Español');
        });
    });

    describe('t() — translation', () => {
        it('translates a simple key in English', () => {
            expect(i18n.t('common.loading')).toBe('Loading...');
        });

        it('translates nav keys', () => {
            expect(i18n.t('nav.settings')).toBe('Settings');
        });

        it('translates with interpolation ({{param}})', () => {
            expect(i18n.t('offline.updated', { time: '5m' })).toBe('Updated 5m ago');
        });

        it('translates with numeric interpolation', () => {
            expect(i18n.t('supply.binsEnRoute', { count: 3 })).toBe('✅ 3 empty bins en route');
        });

        it('uses Samoan translations when language is sm', () => {
            i18n.setLanguage('sm');
            expect(i18n.t('common.loading')).toBe('O loʻo faʻapipiʻi...');
            expect(i18n.t('nav.settings')).toBe('Faʻatulagaga');
        });

        it('returns the key itself when no translation exists', () => {
            expect(i18n.t('completely.nonexistent.key')).toBe('completely.nonexistent.key');
        });

        it('returns key for missing translation in non-EN language too', () => {
            i18n.setLanguage('sm');
            expect(i18n.t('some.missing.key')).toBe('some.missing.key');
        });
    });

    describe('convenience t() function', () => {
        it('delegates to i18n.t()', () => {
            expect(t('common.save')).toBe('Save');
            expect(t('common.cancel')).toBe('Cancel');
        });
    });

    describe('subscribe', () => {
        it('notifies listeners on language change', () => {
            const listener = vi.fn();
            i18n.subscribe(listener);
            i18n.setLanguage('es');
            expect(listener).toHaveBeenCalledTimes(1);
        });

        it('returns unsubscribe function', () => {
            const listener = vi.fn();
            const unsub = i18n.subscribe(listener);
            unsub();
            i18n.setLanguage('sm');
            expect(listener).not.toHaveBeenCalled();
        });

        it('does not notify when setting same language', () => {
            const listener = vi.fn();
            i18n.subscribe(listener);
            i18n.setLanguage('en');
            expect(listener).not.toHaveBeenCalled();
        });
    });
});
