/**
 * Tests for DayConfigModal — TeamLeader day configuration
 * @module components/modals/DayConfigModal.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('@/components/ui/ModalOverlay', () => ({
    default: ({ children }: { children: React.ReactNode }) => <div data-testid="modal">{children}</div>,
}));

import DayConfigModal from './DayConfigModal';

const defaultConfig = {
    orchard: 'El Pedregal - Block 4B',
    variety: 'Lapin',
    targetSize: '28mm+',
    targetColor: 'Dark Red',
    binType: 'Standard' as const,
};

describe('DayConfigModal', () => {
    const onClose = vi.fn();
    const onSave = vi.fn();

    beforeEach(() => { vi.clearAllMocks(); });

    it('renders "Day Configuration" title', () => {
        render(<DayConfigModal config={defaultConfig} onClose={onClose} onSave={onSave} />);
        expect(screen.getByText('Day Configuration')).toBeTruthy();
    });

    it('renders orchard select', () => {
        render(<DayConfigModal config={defaultConfig} onClose={onClose} onSave={onSave} />);
        expect(screen.getByLabelText('Orchard Block')).toBeTruthy();
    });

    it('renders variety select', () => {
        render(<DayConfigModal config={defaultConfig} onClose={onClose} onSave={onSave} />);
        expect(screen.getByLabelText('Variety')).toBeTruthy();
    });

    it('renders bin type options', () => {
        render(<DayConfigModal config={defaultConfig} onClose={onClose} onSave={onSave} />);
        expect(screen.getByText('Standard')).toBeTruthy();
        expect(screen.getByText('Export')).toBeTruthy();
        expect(screen.getByText('Process')).toBeTruthy();
    });

    it('shows quality standards', () => {
        render(<DayConfigModal config={defaultConfig} onClose={onClose} onSave={onSave} />);
        expect(screen.getByText('28mm+')).toBeTruthy();
        expect(screen.getByText('Dark Red')).toBeTruthy();
    });

    it('calls onSave and onClose when save clicked', () => {
        render(<DayConfigModal config={defaultConfig} onClose={onClose} onSave={onSave} />);
        fireEvent.click(screen.getByText('Save Configuration'));
        expect(onSave).toHaveBeenCalledWith(expect.objectContaining({ orchard: 'El Pedregal - Block 4B' }));
        expect(onClose).toHaveBeenCalled();
    });
});
