/**
 * Tests for DaySettingsModal — Manager daily settings (bucket rate, target tons)
 * @module components/modals/DaySettingsModal.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('@/components/ui/ModalOverlay', () => ({
    default: ({ children }: { children: React.ReactNode }) => <div data-testid="modal">{children}</div>,
}));

import DaySettingsModal from './DaySettingsModal';

const defaultSettings = { bucketRate: 6.50, targetTons: 40 };

describe('DaySettingsModal', () => {
    const onClose = vi.fn();
    const onSave = vi.fn();

    beforeEach(() => { vi.clearAllMocks(); });

    it('renders "Day Settings" title', () => {
        render(<DaySettingsModal settings={defaultSettings} onClose={onClose} onSave={onSave} />);
        expect(screen.getByText('Day Settings')).toBeTruthy();
    });

    it('renders bucket rate input with default value', () => {
        render(<DaySettingsModal settings={defaultSettings} onClose={onClose} onSave={onSave} />);
        const input = screen.getByLabelText('Bucket Rate in dollars') as HTMLInputElement;
        expect(input.value).toBe('6.5');
    });

    it('renders target tons input', () => {
        render(<DaySettingsModal settings={defaultSettings} onClose={onClose} onSave={onSave} />);
        const input = screen.getByLabelText('Daily Target in tons') as HTMLInputElement;
        expect(input.value).toBe('40');
    });

    it('shows min wage', () => {
        render(<DaySettingsModal settings={defaultSettings} onClose={onClose} onSave={onSave} />);
        expect(screen.getByText(/\$23.5\/hr/)).toBeTruthy();
    });

    it('calls onSave with updated values', () => {
        render(<DaySettingsModal settings={defaultSettings} onClose={onClose} onSave={onSave} />);
        fireEvent.change(screen.getByLabelText('Bucket Rate in dollars'), { target: { value: '7.00' } });
        fireEvent.click(screen.getByText('Save Settings'));
        expect(onSave).toHaveBeenCalledWith(expect.objectContaining({ bucketRate: 7, targetTons: 40 }));
    });

    it('calls onClose after save', () => {
        render(<DaySettingsModal settings={defaultSettings} onClose={onClose} onSave={onSave} />);
        fireEvent.click(screen.getByText('Save Settings'));
        expect(onClose).toHaveBeenCalled();
    });
});
