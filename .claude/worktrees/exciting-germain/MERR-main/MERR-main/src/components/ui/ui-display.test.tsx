/**
 * Tests for PageHeader and LoadingSkeleton components
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';

// ── PageHeader ──────────────────────────────────────
import PageHeader from './PageHeader';

describe('PageHeader', () => {
    it('renders title and icon', () => {
        render(<PageHeader icon="agriculture" title="Harvest Dashboard" />);
        expect(screen.getByText('Harvest Dashboard')).toBeTruthy();
        expect(screen.getByText('agriculture')).toBeTruthy();
    });

    it('renders subtitle when provided', () => {
        render(<PageHeader icon="star" title="Title" subtitle="Subtitle text" />);
        expect(screen.getByText('Subtitle text')).toBeTruthy();
    });

    it('does not render subtitle when absent', () => {
        render(<PageHeader icon="star" title="Title" />);
        expect(screen.queryByText('Subtitle text')).toBeNull();
    });

    it('renders badges', () => {
        render(<PageHeader icon="star" title="Title" badges={[{ label: 'Active', icon: 'check', color: 'emerald' }]} />);
        expect(screen.getByText('Active')).toBeTruthy();
        expect(screen.getByText('check')).toBeTruthy();
    });

    it('renders action content', () => {
        render(<PageHeader icon="star" title="Title" action={<button>Export</button>} />);
        expect(screen.getByText('Export')).toBeTruthy();
    });

    it('renders children', () => {
        render(<PageHeader icon="star" title="Title"><span>Extra content</span></PageHeader>);
        expect(screen.getByText('Extra content')).toBeTruthy();
    });
});

// ── LoadingSkeleton ─────────────────────────────────
import LoadingSkeleton from './LoadingSkeleton';

describe('LoadingSkeleton', () => {
    it('renders default card skeleton', () => {
        const { container } = render(<LoadingSkeleton />);
        expect(container.querySelectorAll('.animate-shimmer').length).toBeGreaterThan(0);
    });

    it('renders correct count of items', () => {
        const { container } = render(<LoadingSkeleton type="list" count={3} />);
        const listItems = container.querySelectorAll('.rounded-full');
        expect(listItems.length).toBe(3); // 3 avatar circles
    });

    it('renders metric type', () => {
        const { container } = render(<LoadingSkeleton type="metric" />);
        expect(container.querySelectorAll('.animate-shimmer').length).toBe(2);
    });

    it('renders table type', () => {
        const { container } = render(<LoadingSkeleton type="table" />);
        expect(container.querySelectorAll('.animate-shimmer').length).toBe(4);
    });

    it('renders text type', () => {
        const { container } = render(<LoadingSkeleton type="text" />);
        expect(container.querySelectorAll('.animate-shimmer').length).toBe(1);
    });

    it('applies custom className', () => {
        const { container } = render(<LoadingSkeleton type="text" className="my-custom" />);
        expect(container.querySelector('.my-custom')).toBeTruthy();
    });
});
