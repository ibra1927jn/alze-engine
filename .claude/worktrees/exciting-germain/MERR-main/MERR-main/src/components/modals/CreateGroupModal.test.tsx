/**
 * Tests for CreateGroupModal — Create group chat with member selection
 * @module components/modals/CreateGroupModal.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('@/utils/logger', () => ({ logger: { info: vi.fn(), error: vi.fn(), warn: vi.fn() } }));
vi.mock('@/components/ui/ModalOverlay', () => ({
    default: ({ children }: { children: React.ReactNode }) => <div data-testid="modal">{children}</div>,
}));

import CreateGroupModal from './CreateGroupModal';

const members = [
    { id: 'u1', name: 'Alice', role: 'Team Leader' },
    { id: 'u2', name: 'Bob', role: 'Picker' },
    { id: 'u3', name: 'Charlie', role: 'Runner' },
];

describe('CreateGroupModal', () => {
    const onClose = vi.fn();
    const onCreate = vi.fn().mockResolvedValue(undefined);

    beforeEach(() => { vi.clearAllMocks(); });

    it('renders group name input', () => {
        render(<CreateGroupModal onClose={onClose} onCreate={onCreate} availableMembers={members} />);
        expect(screen.getByPlaceholderText('Group name')).toBeTruthy();
    });

    it('renders all available members', () => {
        render(<CreateGroupModal onClose={onClose} onCreate={onCreate} availableMembers={members} />);
        expect(screen.getByText('Alice')).toBeTruthy();
        expect(screen.getByText('Bob')).toBeTruthy();
        expect(screen.getByText('Charlie')).toBeTruthy();
    });

    it('shows member roles', () => {
        render(<CreateGroupModal onClose={onClose} onCreate={onCreate} availableMembers={members} />);
        expect(screen.getByText('Team Leader')).toBeTruthy();
        expect(screen.getByText('Picker')).toBeTruthy();
        expect(screen.getByText('Runner')).toBeTruthy();
    });

    it('shows selected member count', () => {
        render(<CreateGroupModal onClose={onClose} onCreate={onCreate} availableMembers={members} />);
        expect(screen.getByText(/Select Members \(0\)/)).toBeTruthy();
        const checkboxes = screen.getAllByRole('checkbox');
        fireEvent.click(checkboxes[0]);
        expect(screen.getByText(/Select Members \(1\)/)).toBeTruthy();
    });

    it('enables create button when name and members are selected', () => {
        render(<CreateGroupModal onClose={onClose} onCreate={onCreate} availableMembers={members} />);
        fireEvent.change(screen.getByPlaceholderText('Group name'), { target: { value: 'My Team' } });
        const checkboxes = screen.getAllByRole('checkbox');
        fireEvent.click(checkboxes[0]);
        // The create button text is "Create Group" — check it's enabled
        const btns = screen.getAllByRole('button');
        const createBtn = btns.find(b => b.textContent?.includes('Create Group'));
        expect(createBtn?.hasAttribute('disabled')).toBe(false);
    });

    it('toggles member selection', () => {
        render(<CreateGroupModal onClose={onClose} onCreate={onCreate} availableMembers={members} />);
        const checkboxes = screen.getAllByRole('checkbox');
        fireEvent.click(checkboxes[0]); // select
        expect(screen.getByText(/Select Members \(1\)/)).toBeTruthy();
        fireEvent.click(checkboxes[0]); // deselect
        expect(screen.getByText(/Select Members \(0\)/)).toBeTruthy();
    });
});
