/**
 * ============================================
 * EXPORT SERVICE — Unit Tests
 * ============================================
 * Verifies CSV/Xero/PaySauce export formats,
 * payroll calculations, and edge cases.
 * ============================================
 */
import { describe, it, expect } from 'vitest';
import { exportService } from './export.service';
import { MINIMUM_WAGE, PIECE_RATE, type Picker } from '../types';

// ── Test Fixtures ──────────────────────────────
function makePicker(overrides: Partial<Picker> = {}): Picker {
    return {
        id: 'p-001',
        picker_id: 'EMP001',
        name: 'Test Picker',
        avatar: 'TP',
        current_row: 1,
        total_buckets_today: 10,
        hours: 8,
        status: 'active',
        safety_verified: true,
        qcStatus: [1, 1, 1],
        ...overrides,
    };
}

const CREW_3: Picker[] = [
    makePicker({ id: 'p-001', picker_id: 'EMP001', name: 'Alice', total_buckets_today: 20, hours: 8 }),
    makePicker({ id: 'p-002', picker_id: 'EMP002', name: 'Bob', total_buckets_today: 5, hours: 8 }),
    makePicker({ id: 'p-003', picker_id: 'EMP003', name: 'Charlie', total_buckets_today: 0, hours: 4 }),
];

// ── preparePayrollData ─────────────────────────
describe('exportService.preparePayrollData', () => {
    it('calculates piece earnings correctly', () => {
        const data = exportService.preparePayrollData([makePicker({ total_buckets_today: 15, hours: 8 })], '2026-02-12');
        const crew0 = data.crew[0];
        expect(crew0.pieceEarnings).toBe(15 * PIECE_RATE);
    });

    it('calculates minimum top-up when piece earnings < minimum wage', () => {
        // 5 buckets × $6.50 = $32.50 piece earnings
        // 8 hours × $23.50 = $188.00 minimum guarantee
        // Top-up = $188.00 - $32.50 = $155.50
        const data = exportService.preparePayrollData(
            [makePicker({ total_buckets_today: 5, hours: 8 })], '2026-02-12'
        );
        const crew0 = data.crew[0];
        expect(crew0.pieceEarnings).toBe(5 * PIECE_RATE);
        expect(crew0.minimumTopUp).toBe((8 * MINIMUM_WAGE) - (5 * PIECE_RATE));
        expect(crew0.totalEarnings).toBe(crew0.pieceEarnings + crew0.minimumTopUp);
    });

    it('no top-up when piece earnings exceed minimum wage', () => {
        // 30 buckets × $6.50 = $195 piece earnings
        // 8 hours × $23.50 = $188.00 minimum
        // Top-up = 0 (piece > min)
        const data = exportService.preparePayrollData(
            [makePicker({ total_buckets_today: 30, hours: 8 })], '2026-02-12'
        );
        expect(data.crew[0].minimumTopUp).toBe(0);
        expect(data.crew[0].totalEarnings).toBe(30 * PIECE_RATE);
    });

    it('handles 0 hours gracefully', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ total_buckets_today: 10, hours: 0 })], '2026-02-12'
        );
        expect(data.crew[0].hours).toBe(0);
        expect(data.crew[0].minimumTopUp).toBe(0);
        expect(data.crew[0].totalEarnings).toBe(10 * PIECE_RATE);
    });

    it('handles 0 buckets gracefully', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ total_buckets_today: 0, hours: 4 })], '2026-02-12'
        );
        expect(data.crew[0].pieceEarnings).toBe(0);
        expect(data.crew[0].minimumTopUp).toBe(4 * MINIMUM_WAGE);
        expect(data.crew[0].totalEarnings).toBe(4 * MINIMUM_WAGE);
    });

    it('computes summary totals correctly for multi-crew', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');

        expect(data.summary.totalBuckets).toBe(20 + 5 + 0);
        expect(data.summary.totalHours).toBe(8 + 8 + 4);

        const expectedPiece = (20 * PIECE_RATE) + (5 * PIECE_RATE) + 0;
        expect(data.summary.totalPieceEarnings).toBe(expectedPiece);

        // Grand total = sum of all individual totalEarnings
        const grandTotal = data.crew.reduce((s, p) => s + p.totalEarnings, 0);
        expect(data.summary.grandTotal).toBe(grandTotal);
    });

    it('computes average buckets per hour correctly', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');
        const expected = Math.round((25 / 20) * 10) / 10; // 25 buckets / 20 hours
        expect(data.summary.averageBucketsPerHour).toBe(expected);
    });

    it('handles empty crew', () => {
        const data = exportService.preparePayrollData([], '2026-02-12');
        expect(data.crew).toHaveLength(0);
        expect(data.summary.totalBuckets).toBe(0);
        expect(data.summary.grandTotal).toBe(0);
        expect(data.summary.averageBucketsPerHour).toBe(0);
    });
});

// ── generateCSV ────────────────────────────────
describe('exportService.generateCSV', () => {
    it('includes correct headers', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');
        const csv = exportService.generateCSV(data);

        expect(csv).toContain('Employee ID');
        expect(csv).toContain('Name');
        expect(csv).toContain('Buckets');
        expect(csv).toContain('Hours');
        expect(csv).toContain('Piece Earnings (NZD)');
        expect(csv).toContain('Minimum Top-Up (NZD)');
        expect(csv).toContain('Total Earnings (NZD)');
        expect(csv).toContain('Status');
    });

    it('includes date in title row', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');
        const csv = exportService.generateCSV(data);
        expect(csv.startsWith('Payroll Report - 2026-02-12')).toBe(true);
    });

    it('includes all crew members', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');
        const csv = exportService.generateCSV(data);
        expect(csv).toContain('EMP001');
        expect(csv).toContain('EMP002');
        expect(csv).toContain('EMP003');
    });

    it('includes SUMMARY section', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');
        const csv = exportService.generateCSV(data);
        expect(csv).toContain('SUMMARY');
        expect(csv).toContain('Total Buckets');
        expect(csv).toContain('Grand Total');
    });
});

// ── generateXeroCSV ────────────────────────────
describe('exportService.generateXeroCSV', () => {
    it('has correct Xero headers', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');
        const csv = exportService.generateXeroCSV(data);
        const firstLine = csv.split('\n')[0];

        expect(firstLine).toBe('Employee ID,Employee Name,Earnings Rate Name,Hours/Quantity,Rate,Amount');
    });

    it('generates Piece Rate Earnings line for workers with buckets and hours', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ picker_id: 'X01', hours: 8, total_buckets_today: 10 })],
            '2026-02-12'
        );
        const csv = exportService.generateXeroCSV(data);
        // V3 fix: no longer emits "Ordinary Hours" — uses Piece Rate Earnings as primary
        expect(csv).toContain('Piece Rate Earnings');
        expect(csv).toContain(PIECE_RATE.toFixed(2));
    });

    it('does not emit separate Ordinary Hours line (V3 double-pay fix)', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ picker_id: 'X01', hours: 8, total_buckets_today: 10 })],
            '2026-02-12'
        );
        const csv = exportService.generateXeroCSV(data);
        // V3 fix: Ordinary Hours no longer emitted to prevent double-counting
        expect(csv).not.toContain('Ordinary Hours');
        expect(csv).not.toContain('Piece Rate Bonus');
    });

    it('generates Minimum Wage Top-Up line when applicable', () => {
        // 2 buckets at $6.50 = $13 < 8h × $23.50 = $188 → top-up = $175
        const data = exportService.preparePayrollData(
            [makePicker({ picker_id: 'X01', hours: 8, total_buckets_today: 2 })],
            '2026-02-12'
        );
        const csv = exportService.generateXeroCSV(data);
        expect(csv).toContain('Minimum Wage Top-Up');
    });

    it('does NOT generate Top-Up line when piece > minimum', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ picker_id: 'X01', hours: 8, total_buckets_today: 30 })],
            '2026-02-12'
        );
        const csv = exportService.generateXeroCSV(data);
        expect(csv).not.toContain('Minimum Wage Top-Up');
    });

    it('skips Ordinary Hours line when hours = 0', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ picker_id: 'X01', hours: 0, total_buckets_today: 5 })],
            '2026-02-12'
        );
        const csv = exportService.generateXeroCSV(data);
        expect(csv).not.toContain('Ordinary Hours');
    });
});

// ── generatePaySauceCSV ────────────────────────
describe('exportService.generatePaySauceCSV', () => {
    it('has correct PaySauce headers', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');
        const csv = exportService.generatePaySauceCSV(data);
        const firstLine = csv.split('\n')[0];

        expect(firstLine).toBe('Employee Number,Pay Type,Description,Quantity,Rate,Amount');
    });

    it('generates one EARNINGS line per worker', () => {
        const data = exportService.preparePayrollData(CREW_3, '2026-02-12');
        const csv = exportService.generatePaySauceCSV(data);
        const lines = csv.split('\n').filter(l => l.includes('EARNINGS'));
        expect(lines.length).toBe(3); // one per crew member
    });

    it('includes bucket count and hours in description', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ picker_id: 'PS01', total_buckets_today: 15, hours: 7.5 })],
            '2026-02-12'
        );
        const csv = exportService.generatePaySauceCSV(data);
        expect(csv).toContain('15 buckets');
        expect(csv).toContain('7.5h');
    });

    it('handles 0 hours without division by zero', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ picker_id: 'PS01', total_buckets_today: 5, hours: 0 })],
            '2026-02-12'
        );
        // Should not throw
        const csv = exportService.generatePaySauceCSV(data);
        expect(csv).toContain('PS01');
        expect(csv).toContain('EARNINGS');
    });
});

// ── Edge Cases ─────────────────────────────────
describe('Export edge cases', () => {
    it('handles picker with missing picker_id', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ picker_id: '' })], '2026-02-12'
        );
        // Empty string is falsy → falls back to 'N/A' via || operator
        expect(data.crew[0].employeeId).toBe('N/A');
    });

    it('CSV does not crash on special characters in names', () => {
        const data = exportService.preparePayrollData(
            [makePicker({ name: 'O\'Brien, "Mac"' })], '2026-02-12'
        );
        const csv = exportService.generateCSV(data);
        expect(csv).toContain("O'Brien");
    });
});
