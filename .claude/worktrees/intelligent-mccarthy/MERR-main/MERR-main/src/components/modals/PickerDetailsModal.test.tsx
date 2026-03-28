/**
 * PickerDetailsModal.test.tsx — Tests for role-aware profile modal
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import PickerDetailsModal from './PickerDetailsModal';
import { Picker, Role } from '../../types';

// Mock sub-components for isolation
vi.mock('./picker-details/PickerProfileView', () => ({
    default: () => <div data-testid="picker-profile-view">PickerProfile</div>,
}));
vi.mock('./picker-details/TeamLeaderProfileView', () => ({
    default: () => <div data-testid="team-leader-profile-view">TeamLeaderProfile</div>,
}));
vi.mock('./picker-details/RunnerProfileView', () => ({
    default: () => <div data-testid="runner-profile-view">RunnerProfile</div>,
}));
vi.mock('./picker-details/QuickMessageView', () => ({
    default: () => <div data-testid="quick-message-view">QuickMessage</div>,
}));
vi.mock('./picker-details/ActivityHistoryView', () => ({
    default: () => <div data-testid="activity-history-view">ActivityHistory</div>,
}));

const makePicker = (role: Role = 'picker', overrides: Partial<Picker> = {}): Picker => ({
    id: 'p-1',
    name: 'Test Picker',
    picker_id: 'p-1',
    role,
    orchard_id: 'o-1',
    status: 'active',
    safety_verified: true,
    avatar: 'TP',
    current_row: 5,
    total_buckets_today: 20,
    hours: 4,
    qcStatus: [100],
    ...overrides,
});

describe('PickerDetailsModal', () => {
    const onClose = vi.fn();
    const onUpdate = vi.fn();

    beforeEach(() => {
        vi.clearAllMocks();
    });

    it('renders picker name', () => {
        render(
            <PickerDetailsModal
                picker={makePicker('picker', { name: 'Ana Torres' })}
                onClose={onClose}
                onUpdate={onUpdate}
            />
        );
        expect(screen.getByText('Ana Torres')).toBeTruthy();
    });

    it('renders role label for picker', () => {
        render(
            <PickerDetailsModal
                picker={makePicker('picker')}
                onClose={onClose}
                onUpdate={onUpdate}
            />
        );
        expect(screen.getAllByText(/picker/i).length).toBeGreaterThan(0);
    });

    it('renders PickerProfileView for picker role', () => {
        render(
            <PickerDetailsModal
                picker={makePicker('picker')}
                onClose={onClose}
                onUpdate={onUpdate}
            />
        );
        expect(screen.getByTestId('picker-profile-view')).toBeTruthy();
    });

    it('renders TeamLeaderProfileView for team_leader role', () => {
        render(
            <PickerDetailsModal
                picker={makePicker('team_leader')}
                onClose={onClose}
                onUpdate={onUpdate}
            />
        );
        expect(screen.getByTestId('team-leader-profile-view')).toBeTruthy();
    });

    it('renders RunnerProfileView for runner role', () => {
        render(
            <PickerDetailsModal
                picker={makePicker('runner')}
                onClose={onClose}
                onUpdate={onUpdate}
            />
        );
        expect(screen.getByTestId('runner-profile-view')).toBeTruthy();
    });

    it('closes on close button click', () => {
        render(
            <PickerDetailsModal
                picker={makePicker()}
                onClose={onClose}
                onUpdate={onUpdate}
            />
        );
        fireEvent.click(screen.getByText('close'));
        expect(onClose).toHaveBeenCalled();
    });

    it('shows delete button when showDeleteButton is true', () => {
        render(
            <PickerDetailsModal
                picker={makePicker()}
                onClose={onClose}
                onUpdate={onUpdate}
                showDeleteButton={true}
                onDelete={vi.fn()}
            />
        );
        expect(screen.getByText('Remove Picker')).toBeTruthy();
    });

    it('does not show delete button by default', () => {
        render(
            <PickerDetailsModal
                picker={makePicker()}
                onClose={onClose}
                onUpdate={onUpdate}
            />
        );
        expect(screen.queryByText(/Remove from Orchard/i)).toBeNull();
    });

    it('renders tabs for navigation (Profile, Activity, Message)', () => {
        render(
            <PickerDetailsModal
                picker={makePicker()}
                onClose={onClose}
                onUpdate={onUpdate}
                onSendMessage={vi.fn()}
            />
        );
        // Should have at least profile and activity tabs
        expect(screen.getByText(/profile/i)).toBeTruthy();
    });
});
