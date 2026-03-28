/**
 * Tests for ConflictResolver — Scheduling conflict resolution UI
 * @module components/common/ConflictResolver.test
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import ConflictResolver from './ConflictResolver';

const mockConflicts = [
    {
        id: 'c1',
        type: 'double_booking' as const,
        severity: 'critical' as const,
        title: 'Picker A assigned to same row',
        description: 'Row 14 in Block A has two pickers',
        affectedEntities: ['Picker A', 'Picker B'],
        resolutions: [
            { id: 'r1', label: 'Keep Picker A', icon: 'person', recommended: true },
            { id: 'r2', label: 'Keep Picker B', icon: 'person' },
        ],
        detectedAt: '2026-03-05T08:00:00',
    },
];

describe('ConflictResolver', () => {
    const onResolve = vi.fn();
    const onDismiss = vi.fn();

    beforeEach(() => { vi.clearAllMocks(); });

    it('renders conflict title', () => {
        render(<ConflictResolver conflicts={mockConflicts} onResolve={onResolve} onDismiss={onDismiss} />);
        expect(screen.getByText(/Picker A assigned/)).toBeTruthy();
    });

    it('shows active conflict count', () => {
        render(<ConflictResolver conflicts={mockConflicts} onResolve={onResolve} onDismiss={onDismiss} />);
        expect(screen.getByText(/1 Active Conflict/)).toBeTruthy();
    });

    it('shows resolution buttons after expanding', () => {
        render(<ConflictResolver conflicts={mockConflicts} onResolve={onResolve} onDismiss={onDismiss} />);
        // Click the conflict header to expand
        fireEvent.click(screen.getByText(/Picker A assigned/));
        // Now resolution buttons should be visible
        expect(screen.getByText('Keep Picker A')).toBeTruthy();
        expect(screen.getByText('Keep Picker B')).toBeTruthy();
    });

    it('calls onResolve when resolution clicked', () => {
        render(<ConflictResolver conflicts={mockConflicts} onResolve={onResolve} onDismiss={onDismiss} />);
        fireEvent.click(screen.getByText(/Picker A assigned/)); // expand
        fireEvent.click(screen.getByText('Keep Picker A'));  // resolve
        expect(onResolve).toHaveBeenCalledWith('c1', 'r1');
    });

    it('renders "No Active Conflicts" when empty', () => {
        render(<ConflictResolver conflicts={[]} onResolve={onResolve} onDismiss={onDismiss} />);
        expect(screen.getByText(/No Active Conflicts/)).toBeTruthy();
    });
});
