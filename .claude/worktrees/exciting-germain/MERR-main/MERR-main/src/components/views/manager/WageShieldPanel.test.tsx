/**
 * WageShieldPanel.test.tsx — Tests for wage bleeding dashboard
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import WageShieldPanel from './WageShieldPanel';
import { Picker, Role } from '../../../types';

vi.mock('../../../stores/useHarvestStore', () => ({
    useHarvestStore: (selector?: (s: Record<string, unknown>) => unknown) => {
        const state = { bucketRecords: [] };
        if (selector) return selector(state);
        return state;
    },
}));

vi.mock('../../charts/TrendLineChart', () => ({
    TrendLineChart: () => <div data-testid="trend-chart">TrendChart</div>,
}));

const makePicker = (id: string, name: string, buckets: number = 10, hours: number = 4): Picker => ({
    id, name, picker_id: id, role: 'picker' as Role,
    orchard_id: 'o-1', status: 'active', safety_verified: true,
    avatar: name[0], current_row: 1, total_buckets_today: buckets, hours, qcStatus: [100],
});

const defaultSettings = { piece_rate: 6.50, min_wage_rate: 23.50 };

describe('WageShieldPanel', () => {
    beforeEach(() => {
        vi.clearAllMocks();
    });

    it('renders Wage Shield heading', () => {
        render(
            <WageShieldPanel
                crew={[]}
                teamLeaders={[]}
                settings={defaultSettings}
            />
        );
        expect(screen.getByText(/Wage Bleeding/i)).toBeTruthy();
    });

    it('renders with crew data', () => {
        const crew = [
            makePicker('p1', 'Picker A', 15, 6),
            makePicker('p2', 'Picker B', 5, 6),
        ];
        render(
            <WageShieldPanel
                crew={crew}
                teamLeaders={[]}
                settings={defaultSettings}
            />
        );
        // Should render without crashing
        expect(screen.getByText(/Wage Bleeding/i)).toBeTruthy();
    });

    it('renders at-risk pickers', () => {
        // Picker with very few buckets (below min wage threshold)
        const crew = [
            makePicker('p1', 'Slow Worker', 2, 8), // Very low production
        ];
        render(
            <WageShieldPanel
                crew={crew}
                teamLeaders={[]}
                settings={defaultSettings}
            />
        );
        expect(screen.getByText(/Wage Bleeding/i)).toBeTruthy();
    });

    it('handles empty crew', () => {
        render(
            <WageShieldPanel
                crew={[]}
                teamLeaders={[]}
                settings={defaultSettings}
            />
        );
        expect(screen.getByText(/Wage Bleeding/i)).toBeTruthy();
    });
});
