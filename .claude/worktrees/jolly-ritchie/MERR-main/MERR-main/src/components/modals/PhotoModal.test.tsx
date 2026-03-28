/**
 * Tests for PhotoModal — Photo capture and report sending
 * @module components/modals/PhotoModal.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('@/hooks/useToast', () => ({
    useToast: () => ({ showToast: vi.fn() }),
}));
vi.mock('@/components/ui/ModalOverlay', () => ({
    default: ({ children }: { children: React.ReactNode }) => <div data-testid="modal-overlay">{children}</div>,
}));

import PhotoModal from './PhotoModal';

describe('PhotoModal', () => {
    const onClose = vi.fn();
    const onSend = vi.fn();

    beforeEach(() => { vi.clearAllMocks(); });

    it('renders "Photo Report" title', () => {
        render(<PhotoModal onClose={onClose} />);
        expect(screen.getByText('Photo Report')).toBeTruthy();
    });

    it('shows "Tap to capture" initially', () => {
        render(<PhotoModal onClose={onClose} />);
        expect(screen.getByText('Tap to capture')).toBeTruthy();
    });

    it('renders notes textarea', () => {
        render(<PhotoModal onClose={onClose} />);
        expect(screen.getByPlaceholderText(/Damaged bin/)).toBeTruthy();
    });

    it('send button is disabled when no photo and no notes', () => {
        render(<PhotoModal onClose={onClose} />);
        const sendBtn = screen.getByText('Send Report');
        expect(sendBtn.closest('button')?.disabled).toBe(true);
    });

    it('send button enables when notes are added', () => {
        render(<PhotoModal onClose={onClose} />);
        fireEvent.change(screen.getByPlaceholderText(/Damaged bin/), { target: { value: 'Some notes' } });
        const sendBtn = screen.getByText('Send Report');
        expect(sendBtn.closest('button')?.disabled).toBe(false);
    });

    it('calls onSend with notes when report sent', () => {
        render(<PhotoModal onClose={onClose} onSend={onSend} />);
        fireEvent.change(screen.getByPlaceholderText(/Damaged bin/), { target: { value: 'Broken bin' } });
        fireEvent.click(screen.getByText('Send Report'));
        expect(onSend).toHaveBeenCalledWith('Broken bin', false);
    });

    it('calls onClose after sending', () => {
        render(<PhotoModal onClose={onClose} onSend={onSend} />);
        fireEvent.change(screen.getByPlaceholderText(/Damaged bin/), { target: { value: 'test' } });
        fireEvent.click(screen.getByText('Send Report'));
        expect(onClose).toHaveBeenCalled();
    });
});
