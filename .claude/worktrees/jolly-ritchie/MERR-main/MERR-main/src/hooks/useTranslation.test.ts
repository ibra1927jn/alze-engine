/**
 * useTranslation Hook Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook, act } from '@testing-library/react';
import { useTranslation } from './useTranslation';
import { i18n } from '../services/i18n.service';

vi.mock('../services/i18n.service', () => ({
    i18n: {
        getLanguage: vi.fn(() => 'en'),
        setLanguage: vi.fn(),
        getLanguageInfo: vi.fn(() => ({ code: 'en', name: 'English', flag: '🇬🇧' })),
        subscribe: vi.fn(() => vi.fn()), // Returns unsubscribe
    },
    t: vi.fn((key: string) => key),
    SUPPORTED_LANGUAGES: [
        { code: 'en', name: 'English', flag: '🇬🇧' },
        { code: 'mi', name: 'Te Reo Māori', flag: '🇳🇿' },
    ],
}));

describe('useTranslation', () => {
    beforeEach(() => { vi.clearAllMocks(); });

    it('returns t function', () => {
        const { result } = renderHook(() => useTranslation());
        expect(typeof result.current.t).toBe('function');
    });

    it('returns current language', () => {
        const { result } = renderHook(() => useTranslation());
        expect(result.current.language).toBe('en');
    });

    it('returns language info', () => {
        const { result } = renderHook(() => useTranslation());
        expect(result.current.languageInfo).toEqual({ code: 'en', name: 'English', flag: '🇬🇧' });
    });

    it('returns supported languages list', () => {
        const { result } = renderHook(() => useTranslation());
        expect(result.current.languages).toHaveLength(2);
    });

    it('setLanguage calls i18n.setLanguage', () => {
        const { result } = renderHook(() => useTranslation());
        act(() => { result.current.setLanguage('mi' as never); });
        expect(i18n.setLanguage).toHaveBeenCalledWith('mi');
    });

    it('subscribes to language changes on mount', () => {
        renderHook(() => useTranslation());
        expect(i18n.subscribe).toHaveBeenCalledTimes(1);
    });

    it('t function translates keys', () => {
        const { result } = renderHook(() => useTranslation());
        const translated = result.current.t('some.key');
        expect(translated).toBe('some.key');
    });
});
