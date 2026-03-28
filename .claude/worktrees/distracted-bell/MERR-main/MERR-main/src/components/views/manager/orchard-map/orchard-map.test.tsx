/**
 * Tests for orchard-map components: BlockCard, RowCard
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

// Mock utils
vi.mock('@/utils/orchardMapUtils', () => ({
    getBlockStatusColor: vi.fn().mockReturnValue('#f0fdf4'),
    getBlockStatusBorder: vi.fn().mockReturnValue('#86efac'),
    getBlockTextColor: vi.fn().mockReturnValue('#166534'),
    getStatusLabel: vi.fn().mockReturnValue({ label: 'Active', icon: 'play_circle' }),
    getVarietyStyle: vi.fn().mockReturnValue({ bg: '#ecfdf5', text: '#065f46', dot: '#10b981' }),
    getRowGradient: vi.fn().mockReturnValue('#ffffff'),
    AVATAR_COLORS: ['#ef4444', '#3b82f6', '#10b981', '#f59e0b', '#8b5cf6'],
}));

import BlockCard from './BlockCard';

describe('BlockCard', () => {
    const defaultProps = {
        block: {
            id: 'b1', name: 'Block A', status: 'active' as const,
            totalRows: 20, orchard_id: 'o1',
        },
        stats: { activePickers: 5, buckets: 120, completedRows: 8, progress: 0.4 },
        varieties: ['Sweetheart', 'Lapin'],
        index: 0,
        onClick: vi.fn(),
    };

    it('renders block name', () => {
        render(<BlockCard {...defaultProps} />);
        expect(screen.getByText('Block A')).toBeTruthy();
    });

    it('shows active status label', () => {
        render(<BlockCard {...defaultProps} />);
        expect(screen.getByText('Active')).toBeTruthy();
    });

    it('displays variety badges', () => {
        render(<BlockCard {...defaultProps} />);
        expect(screen.getByText('Sweetheart')).toBeTruthy();
        expect(screen.getByText('Lapin')).toBeTruthy();
    });

    it('shows row count', () => {
        render(<BlockCard {...defaultProps} />);
        expect(screen.getByText('Rows')).toBeTruthy();
        expect(screen.getByText('20')).toBeTruthy();
    });

    it('shows picker count', () => {
        render(<BlockCard {...defaultProps} />);
        expect(screen.getByText('Pickers')).toBeTruthy();
        expect(screen.getByText('5')).toBeTruthy();
    });

    it('shows progress text', () => {
        render(<BlockCard {...defaultProps} />);
        expect(screen.getByText('8/20 rows')).toBeTruthy();
    });

    it('calls onClick when clicked', () => {
        const onClick = vi.fn();
        render(<BlockCard {...defaultProps} onClick={onClick} />);
        fireEvent.click(screen.getByText('Block A').closest('button')!);
        expect(onClick).toHaveBeenCalled();
    });

    it('shows View Rows drill-in hint', () => {
        render(<BlockCard {...defaultProps} />);
        expect(screen.getByText('View Rows')).toBeTruthy();
    });
});

import RowCard from './RowCard';

describe('RowCard', () => {
    const defaultProps = {
        rd: {
            rowNum: 5,
            variety: 'Sweetheart',
            buckets: 30,
            progress: 0.6,
            pickers: [],
        },
        index: 0,
        targetBucketsPerRow: 50,
        isDimmed: false,
        rowAssignments: [],
    };

    it('renders row number', () => {
        render(<RowCard {...defaultProps} />);
        expect(screen.getByText('R5')).toBeTruthy();
    });

    it('renders variety badge', () => {
        render(<RowCard {...defaultProps} />);
        expect(screen.getByText('Sweetheart')).toBeTruthy();
    });

    it('shows bucket count', () => {
        render(<RowCard {...defaultProps} />);
        expect(screen.getByText(/30\/50/)).toBeTruthy();
    });

    it('shows Tap to assign hint when empty row', () => {
        const emptyRd = { ...defaultProps.rd, buckets: 0, progress: 0, pickers: [] };
        render(<RowCard {...defaultProps} rd={emptyRd} />);
        expect(screen.getByText('Tap to assign')).toBeTruthy();
    });

    it('calls onRowClick when clicked', () => {
        const onRowClick = vi.fn();
        render(<RowCard {...defaultProps} onRowClick={onRowClick} />);
        fireEvent.click(screen.getByText('R5').closest('button')!);
        expect(onRowClick).toHaveBeenCalledWith(5);
    });

    it('is disabled when isDimmed', () => {
        render(<RowCard {...defaultProps} isDimmed={true} />);
        expect(screen.getByText('R5').closest('button')).toBeDisabled();
    });
});
