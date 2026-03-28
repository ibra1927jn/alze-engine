/**
 * Tests for SendDirectMessageModal — DM modal with recipient selection
 * @module components/modals/SendDirectMessageModal.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('@/components/ui/ModalOverlay', () => ({
    default: ({ children }: { children: React.ReactNode }) => <div data-testid="modal">{children}</div>,
}));

import SendDirectMessageModal from './SendDirectMessageModal';

const recipients = [
    { id: 'u1', name: 'Alice Lead', role: 'Team Leader', department: 'Block A' },
    { id: 'u2', name: 'Bob Mgr', role: 'Manager' },
];

describe('SendDirectMessageModal', () => {
    const onClose = vi.fn();
    const onSend = vi.fn();

    beforeEach(() => { vi.clearAllMocks(); });

    it('renders "Send Direct Message" title', () => {
        render(<SendDirectMessageModal onClose={onClose} onSend={onSend} recipients={recipients} />);
        expect(screen.getByText('Send Direct Message')).toBeTruthy();
    });

    it('lists recipients with names', () => {
        render(<SendDirectMessageModal onClose={onClose} onSend={onSend} recipients={recipients} />);
        expect(screen.getByText('Alice Lead')).toBeTruthy();
        expect(screen.getByText('Bob Mgr')).toBeTruthy();
    });

    it('shows roles and departments', () => {
        render(<SendDirectMessageModal onClose={onClose} onSend={onSend} recipients={recipients} />);
        expect(screen.getByText(/Team Leader/)).toBeTruthy();
        expect(screen.getByText(/Block A/)).toBeTruthy();
    });

    it('renders message textarea', () => {
        render(<SendDirectMessageModal onClose={onClose} onSend={onSend} recipients={recipients} />);
        expect(screen.getByPlaceholderText('Type your message here...')).toBeTruthy();
    });

    it('send button disabled initially', () => {
        render(<SendDirectMessageModal onClose={onClose} onSend={onSend} recipients={recipients} />);
        const sendBtn = screen.getByText('Send Message').closest('button');
        expect(sendBtn?.disabled).toBe(true);
    });

    it('enables send when recipient selected and message entered', () => {
        render(<SendDirectMessageModal onClose={onClose} onSend={onSend} recipients={recipients} />);
        // Select recipient
        const radios = screen.getAllByRole('radio');
        fireEvent.click(radios[0]);
        // Type message
        fireEvent.change(screen.getByPlaceholderText('Type your message here...'), { target: { value: 'Hello!' } });
        const sendBtn = screen.getByText('Send Message').closest('button');
        expect(sendBtn?.disabled).toBe(false);
    });

    it('calls onSend with recipient and message', () => {
        render(<SendDirectMessageModal onClose={onClose} onSend={onSend} recipients={recipients} />);
        fireEvent.click(screen.getAllByRole('radio')[0]);
        fireEvent.change(screen.getByPlaceholderText('Type your message here...'), { target: { value: 'Help needed' } });
        fireEvent.click(screen.getByText('Send Message'));
        expect(onSend).toHaveBeenCalledWith(recipients[0], 'Help needed');
    });

    it('calls onClose after sending', () => {
        render(<SendDirectMessageModal onClose={onClose} onSend={onSend} recipients={recipients} />);
        fireEvent.click(screen.getAllByRole('radio')[0]);
        fireEvent.change(screen.getByPlaceholderText('Type your message here...'), { target: { value: 'Hi' } });
        fireEvent.click(screen.getByText('Send Message'));
        expect(onClose).toHaveBeenCalled();
    });
});
