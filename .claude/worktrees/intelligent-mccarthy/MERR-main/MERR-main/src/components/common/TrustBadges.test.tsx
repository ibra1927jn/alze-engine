/**
 * Tests for TrustBadges — Enterprise status indicators on Dashboard
 * @module components/common/TrustBadges.test
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';

const mocks = vi.hoisted(() => ({
    crew: [] as unknown[],
    bucketRecords: [] as unknown[],
    alerts: [] as unknown[],
}));

vi.mock('../../stores/useHarvestStore', () => ({
    useHarvestStore: (selector: (s: typeof mocks) => unknown) => selector(mocks),
}));

import TrustBadges from './TrustBadges';

describe('TrustBadges', () => {
    it('renders RLS Security badge', () => {
        render(<TrustBadges />);
        expect(screen.getByText('RLS Security Active')).toBeTruthy();
    });

    it('renders Live badge', () => {
        render(<TrustBadges />);
        expect(screen.getByText('Live')).toBeTruthy();
    });

    it('shows Compliant when no alerts', () => {
        mocks.alerts = [];
        render(<TrustBadges />);
        expect(screen.getByText('Compliant')).toBeTruthy();
    });

    it('shows alert count when alerts exist', () => {
        mocks.alerts = [{ id: '1' }, { id: '2' }];
        render(<TrustBadges />);
        expect(screen.getByText('2 alerts')).toBeTruthy();
    });

    it('shows records synced count', () => {
        mocks.bucketRecords = [{ id: '1' }, { id: '2' }, { id: '3' }];
        render(<TrustBadges />);
        expect(screen.getByText(/3.*records synced/)).toBeTruthy();
    });

    it('shows crew count when no records', () => {
        mocks.bucketRecords = [];
        mocks.crew = [{ id: '1' }, { id: '2' }];
        render(<TrustBadges />);
        expect(screen.getByText(/2.*records synced/)).toBeTruthy();
    });
});
