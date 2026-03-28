/**
 * Tests for common components: BottomNav
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import BottomNav, { NavTab } from './BottomNav';

const tabs: NavTab[] = [
    { id: 'dashboard', label: 'Dashboard', icon: 'dashboard' },
    { id: 'teams', label: 'Teams', icon: 'groups' },
    { id: 'map', label: 'Map', icon: 'map', badge: 3 },
];

describe('BottomNav', () => {
    it('renders all tab labels', () => {
        render(<BottomNav tabs={tabs} activeTab="dashboard" onTabChange={vi.fn()} />);
        expect(screen.getByText('Dashboard')).toBeTruthy();
        expect(screen.getByText('Teams')).toBeTruthy();
        expect(screen.getByText('Map')).toBeTruthy();
    });

    it('renders tab icons', () => {
        render(<BottomNav tabs={tabs} activeTab="dashboard" onTabChange={vi.fn()} />);
        expect(screen.getByText('dashboard')).toBeTruthy();
        expect(screen.getByText('groups')).toBeTruthy();
        expect(screen.getByText('map')).toBeTruthy();
    });

    it('calls onTabChange with correct tab id', () => {
        const onChange = vi.fn();
        render(<BottomNav tabs={tabs} activeTab="dashboard" onTabChange={onChange} />);
        fireEvent.click(screen.getByText('Teams'));
        expect(onChange).toHaveBeenCalledWith('teams');
    });

    it('marks active tab with aria-current', () => {
        render(<BottomNav tabs={tabs} activeTab="teams" onTabChange={vi.fn()} />);
        const teamsButton = screen.getByText('Teams').closest('button')!;
        expect(teamsButton.getAttribute('aria-current')).toBe('page');
    });

    it('does not set aria-current on inactive tabs', () => {
        render(<BottomNav tabs={tabs} activeTab="teams" onTabChange={vi.fn()} />);
        const dashButton = screen.getByText('Dashboard').closest('button')!;
        expect(dashButton.getAttribute('aria-current')).toBeNull();
    });

    it('renders badge dot when badge count > 0', () => {
        const { container } = render(<BottomNav tabs={tabs} activeTab="dashboard" onTabChange={vi.fn()} />);
        const badges = container.querySelectorAll('.bg-red-500');
        expect(badges.length).toBeGreaterThanOrEqual(1);
    });
});
