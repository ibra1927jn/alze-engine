/**
 * Tests for weekly-report sub-components — TrendChartCard
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';

// Mock TrendLineChart
vi.mock('@/components/charts/TrendLineChart', () => ({
    TrendLineChart: ({ data }: { data: any[] }) => <div data-testid="mock-chart">{data.length} points</div>,
}));

import TrendChartCard from './TrendChartCard';

describe('TrendChartCard', () => {
    const defaultProps = {
        title: 'Velocity Trend',
        subtitle: 'Daily avg',
        icon: 'trending_up',
        data: [{ label: 'Mon', value: 100 }, { label: 'Tue', value: 120 }],
        colorTheme: 'blue' as const,
        iconBgClass: 'bg-blue-100',
        iconTextClass: 'text-blue-600',
        bgIconClass: 'text-blue-200',
        valueSuffix: '/hr',
        staggerClass: 'stagger-1',
        onPointClick: vi.fn(),
    };

    it('renders title', () => {
        render(<TrendChartCard {...defaultProps} />);
        expect(screen.getByText('Velocity Trend')).toBeTruthy();
    });

    it('renders subtitle', () => {
        render(<TrendChartCard {...defaultProps} />);
        expect(screen.getByText('Daily avg')).toBeTruthy();
    });

    it('renders chart with data', () => {
        render(<TrendChartCard {...defaultProps} />);
        expect(screen.getByTestId('mock-chart')).toBeTruthy();
    });
});
