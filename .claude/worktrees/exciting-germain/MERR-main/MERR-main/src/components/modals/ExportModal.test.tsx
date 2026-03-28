/**
 * ExportModal.test.tsx — Tests for payroll export modal
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import ExportModal from './ExportModal';
import { Picker, Role } from '../../types';

vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn() },
}));
vi.mock('@/utils/nzst', () => ({
    todayNZST: () => '2026-03-05',
}));
vi.mock('../../services/export.service', () => ({
    exportService: {
        preparePayrollData: vi.fn().mockReturnValue({
            summary: { totalBuckets: 100, totalHours: 32, grandTotal: 2500 },
            rows: [],
        }),
        exportToCSV: vi.fn(),
        exportToXero: vi.fn(),
        exportToPaySauce: vi.fn(),
        exportToPDF: vi.fn(),
        exportPayroll: vi.fn().mockResolvedValue(undefined),
    },
}));
vi.mock('@/components/ui/ModalOverlay', () => ({
    default: ({ children }: { children: React.ReactNode }) => (
        <div data-testid="modal-overlay">{children}</div>
    ),
}));

const makeCrew = (count: number): Picker[] =>
    Array.from({ length: count }, (_, i) => ({
        id: `p-${i}`,
        name: `Picker ${i}`,
        picker_id: `p-${i}`,
        role: 'picker' as Role,
        orchard_id: 'o-1',
        status: 'active' as const,
        safety_verified: true,
        avatar: `P${i}`,
        current_row: i + 1,
        total_buckets_today: 10 + i,
        hours: 4,
        qcStatus: [100],
    }));

describe('ExportModal', () => {
    const onClose = vi.fn();

    beforeEach(() => {
        vi.clearAllMocks();
    });

    it('renders export modal heading', () => {
        render(<ExportModal crew={makeCrew(5)} onClose={onClose} />);
        expect(screen.getByText('Export Payroll')).toBeTruthy();
    });

    it('renders format options (CSV, PDF)', () => {
        render(<ExportModal crew={makeCrew(5)} onClose={onClose} />);
        expect(screen.getByText('CSV')).toBeTruthy();
        expect(screen.getByText('PDF')).toBeTruthy();
    });

    it('shows crew count', () => {
        render(<ExportModal crew={makeCrew(8)} onClose={onClose} />);
        expect(screen.getAllByText(/8/).length).toBeGreaterThan(0);
    });

    it('renders close button', () => {
        render(<ExportModal crew={makeCrew(3)} onClose={onClose} />);
        const closeBtn = screen.getByText('close');
        expect(closeBtn).toBeTruthy();
    });

    it('calls onClose when close button clicked', () => {
        render(<ExportModal crew={makeCrew(3)} onClose={onClose} />);
        fireEvent.click(screen.getByText('close'));
        expect(onClose).toHaveBeenCalled();
    });

    it('renders export action button', () => {
        render(<ExportModal crew={makeCrew(3)} onClose={onClose} />);
        expect(screen.getByText(/Export CSV/)).toBeTruthy();
    });

    it('allows selecting CSV format', () => {
        render(<ExportModal crew={makeCrew(3)} onClose={onClose} />);
        const csvOption = screen.getByText('CSV');
        fireEvent.click(csvOption);
        // CSV should be visually selected (no error thrown)
    });

    it('allows selecting PDF format', () => {
        render(<ExportModal crew={makeCrew(3)} onClose={onClose} />);
        const pdfOption = screen.getByText('PDF');
        fireEvent.click(pdfOption);
        // PDF selection should work
    });
});
