/**
 * Tests for NotificationPanel — In-app notification dropdown
 * @module components/common/NotificationPanel.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

const mocks = vi.hoisted(() => ({
    broadcasts: [] as Array<{
        id: string; title: string; content: string; created_at: string;
        acknowledged_by: string[]; sender_id: string;
    }>,
}));

vi.mock('@/services/config.service', () => ({
    getConfig: () => ({ SUPABASE_URL: 'x', SUPABASE_ANON_KEY: 'k' }),
}));
vi.mock('@/services/supabase', () => ({
    supabase: { auth: { getSession: vi.fn() }, from: vi.fn(), channel: vi.fn().mockReturnValue({ on: vi.fn().mockReturnThis(), subscribe: vi.fn() }), removeChannel: vi.fn() },
}));
vi.mock('@/context/MessagingContext', () => ({
    useMessaging: () => ({
        broadcasts: mocks.broadcasts,
        messages: [],
        unreadCount: mocks.broadcasts.length,
    }),
}));

import NotificationPanel from './NotificationPanel';

describe('NotificationPanel', () => {
    beforeEach(() => {
        vi.clearAllMocks();
        mocks.broadcasts = [];
    });

    it('renders panel header', () => {
        render(<NotificationPanel onClose={vi.fn()} />);
        expect(screen.getByText('Notifications')).toBeTruthy();
    });

    it('shows Welcome message when no broadcasts', () => {
        render(<NotificationPanel onClose={vi.fn()} />);
        expect(screen.getByText(/Welcome to HarvestPro/i)).toBeTruthy();
    });

    it('renders View All button when onViewAll provided', () => {
        render(<NotificationPanel onClose={vi.fn()} onViewAll={vi.fn()} />);
        expect(screen.getByText(/View All/i)).toBeTruthy();
    });

    it('calls onViewAll when clicked', () => {
        const onViewAll = vi.fn();
        render(<NotificationPanel onClose={vi.fn()} onViewAll={onViewAll} />);
        fireEvent.click(screen.getByText(/View All/i));
        expect(onViewAll).toHaveBeenCalled();
    });

    it('renders broadcast notifications', () => {
        mocks.broadcasts = [{
            id: 'b1', title: 'Weather Alert', content: 'Heavy rain expected',
            created_at: new Date().toISOString(), acknowledged_by: [], sender_id: 'u1',
        }];
        render(<NotificationPanel onClose={vi.fn()} />);
        expect(screen.getByText('Weather Alert')).toBeTruthy();
    });
});
