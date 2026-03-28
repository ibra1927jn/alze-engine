/**
 * Tests for Header — Shared mobile header with notifications and sign-out
 * 
 * Uses AuthContext, MessagingContext, and react-router
 * @module components/common/Header.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

const mocks = vi.hoisted(() => ({
    signOut: vi.fn(),
    appUser: { full_name: 'Alice Manager', email: 'alice@test.com' },
    unreadCount: 0,
    navigateFn: vi.fn(),
    getPendingCount: vi.fn().mockResolvedValue(0),
}));

vi.mock('@/context/AuthContext', () => ({
    useAuth: () => ({ appUser: mocks.appUser, signOut: mocks.signOut }),
}));
vi.mock('@/context/MessagingContext', () => ({
    useMessaging: () => ({ unreadCount: mocks.unreadCount }),
}));
vi.mock('react-router-dom', () => ({
    useNavigate: () => mocks.navigateFn,
}));
vi.mock('@/services/offline.service', () => ({
    offlineService: { getPendingCount: mocks.getPendingCount },
}));
vi.mock('@/services/config.service', () => ({
    getConfig: () => ({ SUPABASE_URL: 'x', SUPABASE_ANON_KEY: 'k' }),
}));

import { Header } from './Header';

describe('Header', () => {
    beforeEach(() => {
        vi.clearAllMocks();
        mocks.unreadCount = 0;
        mocks.appUser = { full_name: 'Alice Manager', email: 'alice@test.com' };
    });

    it('renders title and subtitle', () => {
        render(<Header title="HarvestPro" subtitle="Manager Dashboard" />);
        expect(screen.getByText('HarvestPro')).toBeTruthy();
        expect(screen.getByText('Manager Dashboard')).toBeTruthy();
    });

    it('displays user initials from full name', () => {
        render(<Header title="HP" subtitle="test" />);
        expect(screen.getByText('AM')).toBeTruthy(); // Alice Manager → AM
    });

    it('renders sign-out button', () => {
        render(<Header title="HP" subtitle="test" />);
        const buttons = screen.getAllByRole('button');
        const signOutBtn = buttons.find(b => b.getAttribute('title') === 'Sign Out');
        expect(signOutBtn).toBeTruthy();
    });

    it('renders notification bell', () => {
        render(<Header title="HP" subtitle="test" />);
        const bellBtn = screen.getAllByRole('button').find(b => b.getAttribute('title') === 'Notifications');
        expect(bellBtn).toBeTruthy();
    });

    it('shows unread badge when count > 0', () => {
        mocks.unreadCount = 5;
        render(<Header title="HP" subtitle="test" />);
        expect(screen.getByText('5')).toBeTruthy();
    });

    it('shows 99+ when unread > 99', () => {
        mocks.unreadCount = 150;
        render(<Header title="HP" subtitle="test" />);
        expect(screen.getByText('99+')).toBeTruthy();
    });

    it('calls onProfileClick when avatar is clicked', () => {
        const onProfile = vi.fn();
        render(<Header title="HP" subtitle="test" onProfileClick={onProfile} />);
        const avatar = screen.getByText('AM');
        fireEvent.click(avatar);
        expect(onProfile).toHaveBeenCalled();
    });

    it('renders header element', () => {
        render(<Header title="HP" subtitle="test" />);
        expect(screen.getByRole('banner')).toBeTruthy();
    });

    it('handles user with single name', () => {
        mocks.appUser = { full_name: 'Admin', email: 'admin@test.com' };
        render(<Header title="HP" subtitle="test" />);
        expect(screen.getByText('A')).toBeTruthy();
    });
});
