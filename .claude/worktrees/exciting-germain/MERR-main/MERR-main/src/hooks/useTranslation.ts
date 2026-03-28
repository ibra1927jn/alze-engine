// =============================================
// useTranslation Hook
// =============================================
// React hook for translations with automatic re-render on language change

import { useState, useEffect, useCallback } from 'react';
import { i18n, t as translate, SupportedLanguage, LanguageInfo, SUPPORTED_LANGUAGES } from '../services/i18n.service';

interface UseTranslationReturn {
    /** Translate a key */
    t: (key: string, params?: Record<string, string | number>) => string;
    /** Current language code */
    language: SupportedLanguage;
    /** Current language info (name, flag, etc.) */
    languageInfo: LanguageInfo;
    /** Change language */
    setLanguage: (lang: SupportedLanguage) => void;
    /** All supported languages */
    languages: LanguageInfo[];
}

export function useTranslation(): UseTranslationReturn {
    const [language, setLanguageState] = useState<SupportedLanguage>(i18n.getLanguage());

    useEffect(() => {
        // Subscribe to language changes
        const unsubscribe = i18n.subscribe(() => {
            setLanguageState(i18n.getLanguage());
        });

        return unsubscribe;
    }, []);

    const setLanguage = useCallback((lang: SupportedLanguage) => {
        i18n.setLanguage(lang);
    }, []);

    const t = useCallback((key: string, params?: Record<string, string | number>) => {
        return translate(key, params);
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [language]); // Re-create when language changes

    return {
        t,
        language,
        languageInfo: i18n.getLanguageInfo(),
        setLanguage,
        languages: SUPPORTED_LANGUAGES,
    };
}

export default useTranslation;
