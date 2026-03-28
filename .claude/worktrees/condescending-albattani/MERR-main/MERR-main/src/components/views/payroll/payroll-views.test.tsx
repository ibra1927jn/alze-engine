/**
 * Tests for ExportHistoryTab (payroll views)
 * Tests both the component rendering and the utility functions
 */
import React from 'react';
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';

vi.mock('@/utils/logger', () => ({
    logger: { warn: vi.fn(), info: vi.fn(), error: vi.fn() },
}));

import ExportHistoryTab, { getExportHistory, logExportToHistory, ExportRecord } from './ExportHistoryTab';

describe('getExportHistory', () => {
    beforeEach(() => localStorage.clear());

    it('returns empty array when no history', () => {
        expect(getExportHistory()).toEqual([]);
    });

    it('returns stored records', () => {
        const records: ExportRecord[] = [{
            id: '1', format: 'csv', date: '2026-03-04',
            filename: 'payroll.csv', recordCount: 10, totalEarnings: 500,
            timestamp: new Date().toISOString(),
        }];
        localStorage.setItem('harvestpro_export_history', JSON.stringify(records));
        expect(getExportHistory()).toHaveLength(1);
        expect(getExportHistory()[0].format).toBe('csv');
    });

    it('returns empty array on corrupted JSON', () => {
        localStorage.setItem('harvestpro_export_history', 'NOT_JSON');
        expect(getExportHistory()).toEqual([]);
    });
});

describe('logExportToHistory', () => {
    beforeEach(() => localStorage.clear());

    it('adds record to history', () => {
        logExportToHistory({
            format: 'pdf', date: '2026-03-04',
            filename: 'payroll.pdf', recordCount: 5, totalEarnings: 250,
        });
        const history = getExportHistory();
        expect(history).toHaveLength(1);
        expect(history[0].format).toBe('pdf');
        expect(history[0]).toHaveProperty('id');
        expect(history[0]).toHaveProperty('timestamp');
    });

    it('prepends new records', () => {
        logExportToHistory({ format: 'csv', date: 'd1', filename: 'f1.csv', recordCount: 1, totalEarnings: 100 });
        logExportToHistory({ format: 'pdf', date: 'd2', filename: 'f2.pdf', recordCount: 2, totalEarnings: 200 });
        const history = getExportHistory();
        expect(history[0].format).toBe('pdf'); // most recent first
    });
});

describe('ExportHistoryTab Component', () => {
    beforeEach(() => localStorage.clear());

    it('shows empty state when no history', () => {
        render(<ExportHistoryTab />);
        expect(screen.getByText('No exports yet')).toBeTruthy();
    });

    it('shows Export History title', () => {
        render(<ExportHistoryTab />);
        expect(screen.getByText('Export History')).toBeTruthy();
    });

    it('shows "0 exports logged" when empty', () => {
        render(<ExportHistoryTab />);
        expect(screen.getByText('0 exports logged')).toBeTruthy();
    });

    it('renders records from localStorage', () => {
        const records: ExportRecord[] = [{
            id: '1', format: 'csv', date: '2026-03-04',
            filename: 'payroll-march.csv', recordCount: 25, totalEarnings: 1500,
            timestamp: '2026-03-04T12:00:00Z',
        }];
        localStorage.setItem('harvestpro_export_history', JSON.stringify(records));
        render(<ExportHistoryTab />);
        expect(screen.getByText('payroll-march.csv')).toBeTruthy();
        expect(screen.getByText(/CSV/)).toBeTruthy();
    });

    it('shows Clear All button when records exist', () => {
        localStorage.setItem('harvestpro_export_history', JSON.stringify([{
            id: '1', format: 'csv', date: '2026-03-04',
            filename: 'f.csv', recordCount: 1, totalEarnings: 100,
            timestamp: '2026-03-04T12:00:00Z',
        }]));
        render(<ExportHistoryTab />);
        expect(screen.getByText('Clear All')).toBeTruthy();
    });
});
