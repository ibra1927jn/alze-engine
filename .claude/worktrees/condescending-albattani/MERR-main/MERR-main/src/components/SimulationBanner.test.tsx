/**
 * SimulationBanner.test.tsx — Tests for simulation mode warning banner
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import { SimulationBanner } from './SimulationBanner';

let mockSimulationMode = false;
vi.mock('@/stores/useHarvestStore', () => ({
    useHarvestStore: (selector: (s: Record<string, unknown>) => unknown) =>
        selector({ simulationMode: mockSimulationMode }),
}));

describe('SimulationBanner', () => {
    it('renders nothing when simulationMode is false', () => {
        mockSimulationMode = false;
        const { container } = render(<SimulationBanner />);
        expect(container.innerHTML).toBe('');
    });

    it('renders warning banner when simulationMode is true', () => {
        mockSimulationMode = true;
        render(<SimulationBanner />);
        expect(screen.getByText(/SIMULATION MODE ACTIVE/)).toBeTruthy();
    });

    it('shows "Test Data Only" message', () => {
        mockSimulationMode = true;
        render(<SimulationBanner />);
        expect(screen.getByText(/Test Data Only/)).toBeTruthy();
    });

    it('renders warning icons', () => {
        mockSimulationMode = true;
        render(<SimulationBanner />);
        expect(screen.getAllByText('warning').length).toBe(2);
    });
});
