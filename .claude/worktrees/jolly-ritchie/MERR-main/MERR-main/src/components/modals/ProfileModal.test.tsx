/**
 * Tests for ProfileModal — User profile settings modal
 * @module components/modals/ProfileModal.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('../../context/AuthContext', () => ({
    useAuth: () => ({ appUser: { full_name: 'Maria Runner', email: 'maria@harvest.nz' } }),
}));
vi.mock('@/components/ui/ModalOverlay', () => ({
    default: ({ children }: { children: React.ReactNode }) => <div data-testid="modal">{children}</div>,
}));

import ProfileModal from './ProfileModal';

describe('ProfileModal', () => {
    const onClose = vi.fn();
    const onLogout = vi.fn();

    beforeEach(() => { vi.clearAllMocks(); });

    it('renders "Profile Settings" title', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} />);
        expect(screen.getByText('Profile Settings')).toBeTruthy();
    });

    it('displays user name', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} />);
        expect(screen.getByText('Maria Runner')).toBeTruthy();
    });

    it('shows user initial', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} />);
        expect(screen.getByText('M')).toBeTruthy(); // First char of "Maria Runner"
    });

    it('displays role label', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} roleLabel="Team Leader" />);
        expect(screen.getByText('Team Leader')).toBeTruthy();
    });

    it('shows email', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} />);
        expect(screen.getByText('maria@harvest.nz')).toBeTruthy();
    });

    it('renders Edit Profile button', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} />);
        expect(screen.getByText('Edit Profile')).toBeTruthy();
    });

    it('renders Logout button', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} />);
        expect(screen.getByText('Logout')).toBeTruthy();
    });

    it('shows stats when provided', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} stats={{ bucketsHandled: 42, binsCompleted: 7 }} />);
        expect(screen.getByText('42')).toBeTruthy();
        expect(screen.getByText('Buckets Moved')).toBeTruthy();
    });

    it('switches to edit mode when Edit clicked', () => {
        render(<ProfileModal onClose={onClose} onLogout={onLogout} />);
        fireEvent.click(screen.getByText('Edit Profile'));
        expect(screen.getByLabelText('Full Name')).toBeTruthy();
        expect(screen.getByText('Save Changes')).toBeTruthy();
    });
});
