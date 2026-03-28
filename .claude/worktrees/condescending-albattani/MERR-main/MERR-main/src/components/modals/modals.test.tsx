/**
 * Tests for modal components: PhotoModal, DaySettingsModal
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

// Mock hooks used by modals
vi.mock('@/hooks/useToast', () => ({
    useToast: () => ({ showToast: vi.fn() }),
}));

// ── PhotoModal ──────────────────────────────────────
import PhotoModal from './PhotoModal';

describe('PhotoModal', () => {
    it('renders dialog element', () => {
        render(<PhotoModal onClose={vi.fn()} />);
        expect(screen.getByRole('dialog')).toBeTruthy();
    });

    it('renders title "Photo Report"', () => {
        render(<PhotoModal onClose={vi.fn()} />);
        expect(screen.getByText('Photo Report')).toBeTruthy();
    });

    it('renders capture area', () => {
        render(<PhotoModal onClose={vi.fn()} />);
        expect(screen.getByText('Tap to capture')).toBeTruthy();
    });

    it('renders Send Report button (disabled initially)', () => {
        render(<PhotoModal onClose={vi.fn()} />);
        const btn = screen.getByText('Send Report');
        expect(btn).toBeTruthy();
        expect(btn.closest('button')).toBeDisabled();
    });

    it('calls onClose on Escape key', () => {
        const onClose = vi.fn();
        render(<PhotoModal onClose={onClose} />);
        fireEvent.keyDown(document, { key: 'Escape' });
        expect(onClose).toHaveBeenCalled();
    });

    it('enables Send button after entering notes', () => {
        render(<PhotoModal onClose={vi.fn()} />);
        const textarea = screen.getByPlaceholderText(/Add notes/i);
        fireEvent.change(textarea, { target: { value: 'Damaged bin' } });
        expect(screen.getByText('Send Report').closest('button')).not.toBeDisabled();
    });
});

// ── DaySettingsModal ────────────────────────────────
import DaySettingsModal from './DaySettingsModal';

describe('DaySettingsModal', () => {
    const defaultProps = {
        onClose: vi.fn(),
        settings: { bucketRate: 3.0, targetTons: 10 },
        onSave: vi.fn(),
    };

    it('renders dialog element', () => {
        render(<DaySettingsModal {...defaultProps} />);
        expect(screen.getByRole('dialog')).toBeTruthy();
    });

    it('calls onClose on Escape', () => {
        const onClose = vi.fn();
        render(<DaySettingsModal {...defaultProps} onClose={onClose} />);
        fireEvent.keyDown(document, { key: 'Escape' });
        expect(onClose).toHaveBeenCalled();
    });
});
