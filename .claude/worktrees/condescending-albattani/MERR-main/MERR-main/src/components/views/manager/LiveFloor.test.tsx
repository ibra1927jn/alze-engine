/**
 * LiveFloor.test.tsx — Real-time scan activity feed tests
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import LiveFloor from './LiveFloor';
import { BucketRecord } from '../../../types';

const makeBucketRecord = (overrides: Partial<BucketRecord> = {}): BucketRecord => ({
    id: crypto.randomUUID(),
    picker_id: 'p-1',
    picker_name: 'Test Picker',
    orchard_id: 'o-1',
    row_number: 5,
    created_at: new Date().toISOString(),
    scanned_at: new Date().toISOString(),
    quality_grade: 'A',
    bin_id: 'B-001',
    scanned_by: 'runner-1',
    ...overrides,
});

describe('LiveFloor', () => {
    it('renders empty state when no bucket records', () => {
        render(<LiveFloor bucketRecords={[]} />);
        expect(screen.getByText('No scans recorded yet today.')).toBeTruthy();
    });

    it('renders "Live Floor" heading', () => {
        render(<LiveFloor bucketRecords={[]} />);
        expect(screen.getByText('Live Floor')).toBeTruthy();
    });

    it('renders "Recent Activity" badge', () => {
        render(<LiveFloor bucketRecords={[]} />);
        expect(screen.getByText('Recent Activity')).toBeTruthy();
    });

    it('renders bucket records with picker name', () => {
        const records = [makeBucketRecord({ picker_name: 'Ana Torres' })];
        render(<LiveFloor bucketRecords={records} />);
        expect(screen.getByText('Ana Torres')).toBeTruthy();
    });

    it('renders "Unknown Picker" when picker_name is missing', () => {
        const records = [makeBucketRecord({ picker_name: undefined })];
        render(<LiveFloor bucketRecords={records} />);
        expect(screen.getByText('Unknown Picker')).toBeTruthy();
    });

    it('shows row number for each record', () => {
        const records = [makeBucketRecord({ row_number: 12 })];
        render(<LiveFloor bucketRecords={records} />);
        expect(screen.getByText('Row 12')).toBeTruthy();
    });

    it('displays "Bucket +1" badge for each record', () => {
        const records = [makeBucketRecord(), makeBucketRecord({ id: 'r2', picker_name: 'Another' })];
        render(<LiveFloor bucketRecords={records} />);
        expect(screen.getAllByText('Bucket +1').length).toBe(2);
    });

    it('shows at most 10 records', () => {
        const records = Array.from({ length: 15 }, (_, i) =>
            makeBucketRecord({ id: `r-${i}`, picker_name: `Picker ${i}` })
        );
        render(<LiveFloor bucketRecords={records} />);
        // Only first 10 should be rendered
        expect(screen.getByText('Picker 0')).toBeTruthy();
        expect(screen.getByText('Picker 9')).toBeTruthy();
        expect(screen.queryByText('Picker 10')).toBeNull();
    });

    it('renders picker initial as avatar', () => {
        const records = [makeBucketRecord({ picker_name: 'Maria Garcia' })];
        render(<LiveFloor bucketRecords={records} />);
        expect(screen.getByText('M')).toBeTruthy();
    });

    it('calls onUserSelect when clicking a record', () => {
        const onUserSelect = vi.fn();
        const records = [makeBucketRecord({ picker_id: 'p-42', picker_name: 'Sione' })];
        render(<LiveFloor bucketRecords={records} onUserSelect={onUserSelect} />);
        fireEvent.click(screen.getByText('Sione'));
        expect(onUserSelect).toHaveBeenCalledWith(expect.objectContaining({
            id: 'p-42',
            picker_id: 'p-42',
            name: 'Sione',
            role: 'picker',
        }));
    });

    it('does not crash when onUserSelect is not provided', () => {
        const records = [makeBucketRecord()];
        render(<LiveFloor bucketRecords={records} />);
        // Click should not throw
        fireEvent.click(screen.getByText('Test Picker'));
    });

    it('renders green pulse dot for live indicator', () => {
        const { container } = render(<LiveFloor bucketRecords={[]} />);
        const pulse = container.querySelector('.animate-pulse');
        expect(pulse).toBeTruthy();
    });
});
