// =============================================
// I18N SERVICE - Multi-language Support
// =============================================
// Supports: English (en), Spanish (es), Samoan (sm), Tongan (to)

import { logger } from '@/utils/logger';
import { translations as en } from './translations/en';
import { translations as es } from './translations/es';
import { translations as sm } from './translations/sm';
import { translations as to } from './translations/to';

export type SupportedLanguage = 'en' | 'es' | 'sm' | 'to';

export interface LanguageInfo {
    code: SupportedLanguage;
    name: string;
    nativeName: string;
    flag: string;
}

export const SUPPORTED_LANGUAGES: LanguageInfo[] = [
    { code: 'en', name: 'English', nativeName: 'English', flag: '🇳🇿' },
    { code: 'es', name: 'Spanish', nativeName: 'Español', flag: '🇪🇸' },
    { code: 'sm', name: 'Samoan', nativeName: 'Gagana Samoa', flag: '🇼🇸' },
    { code: 'to', name: 'Tongan', nativeName: 'Lea Faka-Tonga', flag: '🇹🇴' },
];

const translationMap: Record<SupportedLanguage, Record<string, string>> = {
    en,
    es,
    sm,
    to,
};

const STORAGE_KEY = 'harvestpro_language';

class I18nService {
    private currentLanguage: SupportedLanguage = 'en';
    private listeners: Set<() => void> = new Set();

    constructor() {
        this.loadSavedLanguage();
    }

    private loadSavedLanguage(): void {
        try {
            const saved = localStorage.getItem(STORAGE_KEY);
            if (saved && this.isValidLanguage(saved)) {
                this.currentLanguage = saved;
            } else {
                // Detect from browser
                const browserLang = navigator.language.split('-')[0];
                if (this.isValidLanguage(browserLang)) {
                    this.currentLanguage = browserLang;
                }
            }
        } catch (e) {
            logger.warn('[i18n] localStorage not available for language detection:', e);
        }
    }

    private isValidLanguage(lang: string): lang is SupportedLanguage {
        return ['en', 'es', 'sm', 'to'].includes(lang);
    }

    getLanguage(): SupportedLanguage {
        return this.currentLanguage;
    }

    getLanguageInfo(): LanguageInfo {
        return SUPPORTED_LANGUAGES.find(l => l.code === this.currentLanguage) || SUPPORTED_LANGUAGES[0];
    }

    setLanguage(lang: SupportedLanguage): void {
        if (this.currentLanguage === lang) return;

        this.currentLanguage = lang;
        try {
            localStorage.setItem(STORAGE_KEY, lang);
        } catch (e) {
            logger.warn('[i18n] Failed to save language preference:', e);
        }

        // Notify listeners
        this.listeners.forEach(listener => listener());
    }

    /**
     * Translate a key to the current language
     * Supports nested keys with dot notation: 'nav.home'
     * Supports interpolation: t('greeting', { name: 'John' }) => 'Hello, John!'
     */
    t(key: string, params?: Record<string, string | number>): string {
        const translations = translationMap[this.currentLanguage];
        let text = translations[key];

        // Fallback to English if not found
        if (!text && this.currentLanguage !== 'en') {
            text = translationMap.en[key];
        }

        // Return key if no translation found
        if (!text) {
            logger.warn(`[i18n] Missing translation: ${key}`);
            return key;
        }

        // Handle interpolation: {{name}}
        if (params) {
            Object.entries(params).forEach(([param, value]) => {
                text = text.replace(new RegExp(`{{${param}}}`, 'g'), String(value));
            });
        }

        return text;
    }

    /**
     * Subscribe to language changes
     */
    subscribe(listener: () => void): () => void {
        this.listeners.add(listener);
        return () => this.listeners.delete(listener);
    }
}

// Singleton instance
export const i18n = new I18nService();

// Convenience function
export const t = (key: string, params?: Record<string, string | number>): string => {
    return i18n.t(key, params);
};
