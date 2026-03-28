/**
 * LanguageSelector.test.tsx — Tests for language dropdown
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import LanguageSelector from './LanguageSelector';

const mockSetLanguage = vi.fn();
vi.mock('../hooks/useTranslation', () => ({
    useTranslation: () => ({
        language: 'en',
        languageInfo: { code: 'en', flag: '🇳🇿', name: 'English', nativeName: 'English' },
        setLanguage: mockSetLanguage,
        languages: [
            { code: 'en', flag: '🇳🇿', name: 'English', nativeName: 'English' },
            { code: 'es', flag: '🇪🇸', name: 'Spanish', nativeName: 'Español' },
            { code: 'mi', flag: '🇳🇿', name: 'Māori', nativeName: 'Te Reo Māori' },
        ],
    }),
}));

describe('LanguageSelector', () => {
    beforeEach(() => {
        vi.clearAllMocks();
    });

    it('renders trigger button with flag', () => {
        render(<LanguageSelector />);
        expect(screen.getByText('🇳🇿')).toBeTruthy();
    });

    it('renders language code in non-compact mode', () => {
        render(<LanguageSelector />);
        expect(screen.getByText('EN')).toBeTruthy();
    });

    it('opens dropdown on click', () => {
        render(<LanguageSelector />);
        fireEvent.click(screen.getByLabelText('Select language'));
        expect(screen.getByText('Español')).toBeTruthy();
    });

    it('shows all language options when open', () => {
        render(<LanguageSelector />);
        fireEvent.click(screen.getByLabelText('Select language'));
        expect(screen.getAllByText('English').length).toBeGreaterThan(0);
        expect(screen.getByText('Español')).toBeTruthy();
        expect(screen.getByText('Te Reo Māori')).toBeTruthy();
    });

    it('calls setLanguage when selecting a language', () => {
        render(<LanguageSelector />);
        fireEvent.click(screen.getByLabelText('Select language'));
        fireEvent.click(screen.getByText('Español'));
        expect(mockSetLanguage).toHaveBeenCalledWith('es');
    });

    it('shows check mark on active language', () => {
        render(<LanguageSelector />);
        fireEvent.click(screen.getByLabelText('Select language'));
        expect(screen.getByText('check')).toBeTruthy();
    });

    it('hides language name in compact mode', () => {
        render(<LanguageSelector compact />);
        expect(screen.queryByText('EN')).toBeNull();
    });
});
