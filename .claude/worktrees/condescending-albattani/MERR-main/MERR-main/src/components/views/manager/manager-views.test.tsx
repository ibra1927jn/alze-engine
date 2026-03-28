/**
 * Tests for manager view sub-components (pure presentational)
 * KpiCards, GoalProgress, ProgressRing
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';

// ── KpiCards ─────────────────────────────────────────
import KpiCards from './weekly-report/KpiCards';

describe('KpiCards', () => {
    const cards = [
        { icon: 'agriculture', label: 'Total Buckets', value: '1,234', gradient: 'from-blue-50 to-blue-100', iconBg: 'bg-blue-500' },
        { icon: 'scale', label: 'Total Tons', value: '51.4', gradient: 'from-green-50 to-green-100', iconBg: 'bg-green-500' },
    ];

    it('renders all cards', () => {
        render(<KpiCards cards={cards} />);
        expect(screen.getByText('Total Buckets')).toBeTruthy();
        expect(screen.getByText('Total Tons')).toBeTruthy();
    });

    it('displays card values', () => {
        render(<KpiCards cards={cards} />);
        expect(screen.getByText('1,234')).toBeTruthy();
        expect(screen.getByText('51.4')).toBeTruthy();
    });

    it('renders icons', () => {
        render(<KpiCards cards={cards} />);
        expect(screen.getByText('agriculture')).toBeTruthy();
        expect(screen.getByText('scale')).toBeTruthy();
    });

    it('handles empty cards array', () => {
        const { container } = render(<KpiCards cards={[]} />);
        expect(container.querySelector('.grid')).toBeTruthy();
    });
});

// ── ProgressRing ─────────────────────────────────────
import ProgressRing from './orchard-map/ProgressRing';

describe('ProgressRing', () => {
    it('renders an SVG', () => {
        const { container } = render(<ProgressRing progress={0.75} />);
        expect(container.querySelector('svg')).toBeTruthy();
    });

    it('displays percentage text', () => {
        render(<ProgressRing progress={0.75} />);
        expect(screen.getByText('75%')).toBeTruthy();
    });

    it('handles 0% progress', () => {
        render(<ProgressRing progress={0} />);
        expect(screen.getByText('0%')).toBeTruthy();
    });

    it('handles 100% progress', () => {
        render(<ProgressRing progress={1} />);
        expect(screen.getByText('100%')).toBeTruthy();
    });

    it('accepts custom size', () => {
        const { container } = render(<ProgressRing progress={0.5} size={80} />);
        const svg = container.querySelector('svg');
        expect(svg?.getAttribute('width')).toBe('80');
    });
});

// ── GoalProgress ─────────────────────────────────────
import GoalProgress from './GoalProgress';

describe('GoalProgress', () => {
    const defaultProps = {
        progress: 65,
        currentTons: 6.5,
        targetTons: 10,
        eta: '3:30 PM',
        etaStatus: 'on_track' as const,
    };

    it('renders Daily Target label', () => {
        render(<GoalProgress {...defaultProps} />);
        expect(screen.getByText('Daily Target')).toBeTruthy();
    });

    it('shows progress percentage', () => {
        render(<GoalProgress {...defaultProps} />);
        expect(screen.getByText(/65%/)).toBeTruthy();
    });

    it('shows tons progress', () => {
        render(<GoalProgress {...defaultProps} />);
        expect(screen.getByText('6.5 / 10 t')).toBeTruthy();
    });

    it('shows ETA', () => {
        render(<GoalProgress {...defaultProps} />);
        expect(screen.getByText('ETA: 3:30 PM')).toBeTruthy();
    });

    it('shows projection when hoursElapsed > 0.5', () => {
        render(<GoalProgress {...defaultProps} hoursElapsed={4} totalBuckets={200} />);
        expect(screen.getByText(/Projected end-of-day/)).toBeTruthy();
    });

    it('hides projection when hoursElapsed <= 0.5', () => {
        render(<GoalProgress {...defaultProps} hoursElapsed={0.3} />);
        expect(screen.queryByText(/Projected end-of-day/)).toBeNull();
    });
});
