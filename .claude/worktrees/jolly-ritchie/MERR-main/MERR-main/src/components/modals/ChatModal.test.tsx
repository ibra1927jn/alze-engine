/**
 * Tests for ChatModal — Fullscreen chat window
 * @module components/modals/ChatModal.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import ChatModal from './ChatModal';

const mockChat = {
    id: 'chat-1',
    name: 'Harvest Team Alpha',
    isGroup: true,
    members: ['Alice', 'Bob', 'Charlie'],
    lastMsg: 'Starting row 14',
    time: '14:20',
};

describe('ChatModal', () => {
    const onClose = vi.fn();
    const onSendMessage = vi.fn();

    beforeEach(() => { vi.clearAllMocks(); });

    it('renders chat name', () => {
        render(<ChatModal chat={mockChat} onClose={onClose} />);
        expect(screen.getByText('Harvest Team Alpha')).toBeTruthy();
    });

    it('shows member count for group chats', () => {
        render(<ChatModal chat={mockChat} onClose={onClose} />);
        expect(screen.getByText(/3 members/)).toBeTruthy();
    });

    it('shows "Direct message" for non-group chats', () => {
        const dmChat = { id: 'dm-1', name: 'Alice' };
        render(<ChatModal chat={dmChat} onClose={onClose} />);
        expect(screen.getByText('Direct message')).toBeTruthy();
    });

    it('renders message input', () => {
        render(<ChatModal chat={mockChat} onClose={onClose} />);
        expect(screen.getByPlaceholderText('Type a message...')).toBeTruthy();
    });

    it('calls onClose when back button clicked', () => {
        render(<ChatModal chat={mockChat} onClose={onClose} />);
        const backBtns = screen.getAllByRole('button');
        fireEvent.click(backBtns[0]); // First button is back
        expect(onClose).toHaveBeenCalled();
    });

    it('sends message and calls onSendMessage', () => {
        render(<ChatModal chat={mockChat} onClose={onClose} onSendMessage={onSendMessage} />);
        const input = screen.getByPlaceholderText('Type a message...');
        fireEvent.change(input, { target: { value: 'Hello team!' } });
        // Find send button (has send icon)
        const btns = screen.getAllByRole('button');
        const sendBtn = btns.find(b => !b.hasAttribute('disabled') && b.textContent?.includes('send'));
        if (sendBtn) fireEvent.click(sendBtn);
        expect(onSendMessage).toHaveBeenCalledWith('Hello team!');
    });

    it('renders initial messages', () => {
        const messages = [
            { id: 1, sender: 'Alice', text: 'Ready for picking', time: '08:00', isMe: false },
        ];
        render(<ChatModal chat={mockChat} onClose={onClose} initialMessages={messages} />);
        expect(screen.getByText('Ready for picking')).toBeTruthy();
    });

    it('send button is disabled when input empty', () => {
        render(<ChatModal chat={mockChat} onClose={onClose} />);
        const btns = screen.getAllByRole('button');
        const sendBtn = btns[btns.length - 1]; // Last button
        expect(sendBtn.hasAttribute('disabled')).toBe(true);
    });
});
