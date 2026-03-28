/**
 * export-payroll-formats.service.ts
 * NZ Payroll format adapters: Xero & PaySauce CSV generation
 */
import { PIECE_RATE } from '../types';
import type { PayrollExportData } from './export.service';

// CSV cell sanitizer (protect against formula injection)
const escCsv = (val: string): string => {
    const s = String(val);
    if (/^[=+\-@\t\r]/.test(s)) return `'${s}`;
    if (/[,"\n]/.test(s)) return `"${s.replace(/"/g, '""')}"`;
    return s;
};

// =============================================
// XERO PAYROLL FORMAT
// =============================================

/**
 * Generate CSV in Xero Payroll import format.
 * Xero expects: Employee ID, Employee Name, Earnings Rate Name, Hours/Quantity, Rate, Amount
 */
export function generateXeroCSV(data: PayrollExportData): string {
    const headers = [
        'Employee ID', 'Employee Name', 'Earnings Rate Name',
        'Hours/Quantity', 'Rate', 'Amount',
    ];

    const rows: string[][] = [];

    // 🔧 V3: Fixed double-pay — workers earn the GREATER of piece rate vs min wage.
    data.crew.forEach(p => {
        // Piece rate earnings (primary pay line)
        if (p.buckets > 0) {
            rows.push([
                escCsv(p.employeeId), escCsv(p.name), 'Piece Rate Earnings',
                p.buckets.toString(),
                (p.buckets > 0 ? (p.pieceEarnings / p.buckets) : PIECE_RATE).toFixed(2),
                p.pieceEarnings.toFixed(2),
            ]);
        }
        // Minimum wage top-up (only if piece rate < min wage guarantee)
        if (p.minimumTopUp > 0) {
            rows.push([
                escCsv(p.employeeId), escCsv(p.name), 'Minimum Wage Top-Up',
                p.hours.toFixed(2),
                (p.minimumTopUp / Math.max(p.hours, 0.01)).toFixed(2),
                p.minimumTopUp.toFixed(2),
            ]);
        }
    });

    return [headers.join(','), ...rows.map(r => r.join(','))].join('\n');
}

// =============================================
// PAYSAUCE FORMAT
// =============================================

/**
 * Generate CSV in PaySauce import format.
 * PaySauce expects: Employee Number, Pay Type, Description, Quantity, Rate, Amount
 */
export function generatePaySauceCSV(data: PayrollExportData): string {
    const headers = [
        'Employee Number', 'Pay Type', 'Description', 'Quantity', 'Rate', 'Amount',
    ];

    const rows: string[][] = [];

    data.crew.forEach(p => {
        if (p.hours <= 0 && p.totalEarnings <= 0 && p.buckets <= 0) return;
        const effectiveRate = p.hours > 0 ? (p.totalEarnings / p.hours).toFixed(2) : '0.00';
        rows.push([
            escCsv(p.employeeId), 'EARNINGS',
            escCsv(`Harvest work - ${p.buckets} buckets in ${p.hours.toFixed(1)}h`),
            p.hours > 0 ? p.hours.toFixed(2) : '0',
            effectiveRate, p.totalEarnings.toFixed(2),
        ]);
    });

    return [headers.join(','), ...rows.map(r => r.join(','))].join('\n');
}
