/**
 * Tests for SimulationBanner and HeroPanel
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';

// Mock store for SimulationBanner
vi.mock('@/stores/useHarvestStore', () => ({
    useHarvestStore: vi.fn(),
}));

import { SimulationBanner } from '../SimulationBanner';
import { useHarvestStore } from '@/stores/useHarvestStore';

describe('SimulationBanner', () => {
    it('renders nothing when simulationMode is false', () => {
        vi.mocked(useHarvestStore).mockImplementation((selector: any) =>
            selector({ simulationMode: false })
        );
        const { container } = render(<SimulationBanner />);
        expect(container.firstChild).toBeNull();
    });

    it('renders banner when simulationMode is true', () => {
        vi.mocked(useHarvestStore).mockImplementation((selector: any) =>
            selector({ simulationMode: true })
        );
        render(<SimulationBanner />);
        expect(screen.getByText(/SIMULATION MODE ACTIVE/)).toBeTruthy();
    });

    it('shows test data warning text', () => {
        vi.mocked(useHarvestStore).mockImplementation((selector: any) =>
            selector({ simulationMode: true })
        );
        render(<SimulationBanner />);
        expect(screen.getByText('Test Data Only - Not Real Production Data')).toBeTruthy();
    });
});

// ── HeroPanel (auth/login) ──────────────────────────
import HeroPanel from '../auth/login/HeroPanel';

describe('HeroPanel', () => {
    it('renders app title', () => {
        render(<HeroPanel />);
        expect(screen.getByText('HarvestPro')).toBeTruthy();
    });

    it('renders NZ reference', () => {
        render(<HeroPanel />);
        expect(screen.getByText('NZ')).toBeTruthy();
    });
});
