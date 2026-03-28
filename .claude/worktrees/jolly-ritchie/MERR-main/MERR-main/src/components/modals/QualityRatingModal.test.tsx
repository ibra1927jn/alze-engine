/**
 * Tests for QualityRatingModal — Quality grade selection (49L, pure props)
 * @module components/modals/QualityRatingModal.test
 */
import React from 'react';
import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import QualityRatingModal from './QualityRatingModal';

describe('QualityRatingModal', () => {
    const onRate = vi.fn();
    const onCancel = vi.fn();

    it('renders Quality Check title', () => {
        render(<QualityRatingModal scannedCode="BKT-001" onRate={onRate} onCancel={onCancel} />);
        expect(screen.getByText('Quality Check')).toBeTruthy();
    });

    it('displays scanned code', () => {
        render(<QualityRatingModal scannedCode="BKT-001" onRate={onRate} onCancel={onCancel} />);
        expect(screen.getByText(/BKT-001/)).toBeTruthy();
    });

    it('renders all 4 grade buttons', () => {
        render(<QualityRatingModal scannedCode="BKT-001" onRate={onRate} onCancel={onCancel} />);
        expect(screen.getByText('Class A')).toBeTruthy();
        expect(screen.getByText('Class B')).toBeTruthy();
        expect(screen.getByText('Class C')).toBeTruthy();
        expect(screen.getByText('REJECT')).toBeTruthy();
    });

    it('calls onRate("A") when Class A clicked', () => {
        render(<QualityRatingModal scannedCode="BKT-001" onRate={onRate} onCancel={onCancel} />);
        fireEvent.click(screen.getByText('Class A'));
        expect(onRate).toHaveBeenCalledWith('A');
    });

    it('calls onRate("B") when Class B clicked', () => {
        render(<QualityRatingModal scannedCode="BKT-001" onRate={onRate} onCancel={onCancel} />);
        fireEvent.click(screen.getByText('Class B'));
        expect(onRate).toHaveBeenCalledWith('B');
    });

    it('calls onRate("reject") when REJECT clicked', () => {
        render(<QualityRatingModal scannedCode="BKT-001" onRate={onRate} onCancel={onCancel} />);
        fireEvent.click(screen.getByText('REJECT'));
        expect(onRate).toHaveBeenCalledWith('reject');
    });

    it('calls onCancel when Cancel clicked', () => {
        render(<QualityRatingModal scannedCode="BKT-001" onRate={onRate} onCancel={onCancel} />);
        fireEvent.click(screen.getByText('Cancel Scan'));
        expect(onCancel).toHaveBeenCalled();
    });

    it('shows grade descriptions', () => {
        render(<QualityRatingModal scannedCode="BKT-001" onRate={onRate} onCancel={onCancel} />);
        expect(screen.getByText('Perfect')).toBeTruthy();
        expect(screen.getByText('Bin Dump')).toBeTruthy();
    });
});
