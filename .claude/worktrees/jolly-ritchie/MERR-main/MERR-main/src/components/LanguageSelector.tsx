// =============================================
// LANGUAGE SELECTOR COMPONENT
// =============================================
// Dropdown for selecting app language with flag icons

import React, { useState, useRef, useEffect } from 'react';
import { useTranslation } from '../hooks/useTranslation';

interface LanguageSelectorProps {
    /** Compact mode shows only flag */
    compact?: boolean;
    /** Dark theme variant */
    dark?: boolean;
}

export const LanguageSelector: React.FC<LanguageSelectorProps> = ({
    compact = false,
    dark = false
}) => {
    const { language, languageInfo, setLanguage, languages } = useTranslation();
    const [isOpen, setIsOpen] = useState(false);
    const dropdownRef = useRef<HTMLDivElement>(null);

    // Close dropdown when clicking outside
    useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
                setIsOpen(false);
            }
        };

        document.addEventListener('mousedown', handleClickOutside);
        return () => document.removeEventListener('mousedown', handleClickOutside);
    }, []);

    const baseButtonClasses = dark
        ? 'bg-slate-100 hover:bg-slate-200 text-text-main border-border-light'
        : 'bg-white hover:bg-slate-50 text-text-main border-border-light';

    const dropdownClasses = dark
        ? 'bg-white border-border-light shadow-2xl'
        : 'bg-white border-border-light shadow-lg';

    const itemClasses = dark
        ? 'hover:bg-slate-50 text-text-main'
        : 'hover:bg-slate-50 text-text-main';

    const activeItemClasses = dark
        ? 'bg-primary/10 text-primary'
        : 'bg-red-50 text-primary';

    return (
        <div className="relative" ref={dropdownRef}>
            {/* Trigger Button */}
            <button
                onClick={() => setIsOpen(!isOpen)}
                className={`
                    flex items-center gap-2 px-3 py-2 rounded-xl border
                    transition-all duration-200 active:scale-95
                    ${baseButtonClasses}
                    ${compact ? 'px-2.5' : ''}
                `}
                aria-label="Select language"
            >
                <span className="text-lg leading-none">{languageInfo.flag}</span>
                {!compact && (
                    <>
                        <span className="text-sm font-medium">{languageInfo.code.toUpperCase()}</span>
                        <span className="material-symbols-outlined text-[16px] opacity-60">
                            {isOpen ? 'expand_less' : 'expand_more'}
                        </span>
                    </>
                )}
            </button>

            {/* Dropdown Menu */}
            {isOpen && (
                <div
                    className={`
                        absolute right-0 mt-2 w-48 rounded-xl border overflow-hidden z-50
                        animate-in fade-in slide-in-from-top-2 duration-200
                        ${dropdownClasses}
                    `}
                >
                    <div className="py-1">
                        {languages.map((lang) => {
                            const isActive = lang.code === language;
                            return (
                                <button
                                    key={lang.code}
                                    onClick={() => {
                                        setLanguage(lang.code);
                                        setIsOpen(false);
                                    }}
                                    className={`
                                        w-full flex items-center gap-3 px-4 py-3
                                        transition-colors duration-150
                                        ${isActive ? activeItemClasses : itemClasses}
                                    `}
                                >
                                    <span className="text-xl leading-none">{lang.flag}</span>
                                    <div className="flex-1 text-left">
                                        <p className="font-medium text-sm">{lang.nativeName}</p>
                                        <p className={`text-xs ${dark ? 'text-text-muted' : 'text-text-muted'}`}>
                                            {lang.name}
                                        </p>
                                    </div>
                                    {isActive && (
                                        <span className="material-symbols-outlined text-primary text-xl">
                                            check
                                        </span>
                                    )}
                                </button>
                            );
                        })}
                    </div>
                </div>
            )}
        </div>
    );
};

export default LanguageSelector;
