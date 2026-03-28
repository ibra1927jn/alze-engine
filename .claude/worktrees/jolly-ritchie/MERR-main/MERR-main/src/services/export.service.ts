// =============================================
// EXPORT SERVICE - CSV/PDF Generation for Payroll
// =============================================
import { Picker, MINIMUM_WAGE, PIECE_RATE } from '../types';
import { todayNZST } from '@/utils/nzst';
import { generateXeroCSV, generatePaySauceCSV } from './export-payroll-formats.service';
import { generatePDFContent } from './export-pdf-template.service';

// 🔧 V2: Sanitize CSV cells to prevent formula injection in Excel/Sheets
const escCsv = (val: string): string => {
  const s = String(val);
  if (/^[=+\-@\t\r]/.test(s)) return `'${s}`;
  // Wrap in quotes if contains comma, quote, or newline
  if (/[,"\n]/.test(s)) return `"${s.replace(/"/g, '""')}"`;
  return s;
};


// Types
export interface PayrollExportData {
  date: string;
  crew: Array<{
    id: string;
    name: string;
    employeeId: string;
    buckets: number;
    hours: number;
    pieceEarnings: number;
    minimumTopUp: number;
    totalEarnings: number;
    status: string;
  }>;
  summary: {
    totalBuckets: number;
    totalHours: number;
    totalPieceEarnings: number;
    totalMinimumTopUp: number;
    grandTotal: number;
    averageBucketsPerHour: number;
  };
}

export interface ExportOptions {
  format: 'csv' | 'pdf';
  includeDetails: boolean;
  dateRange?: { start: string; end: string };
}

// =============================================
// EXPORT SERVICE
// =============================================
export const exportService = {
  /**
   * Prepare payroll data for export
   * 🔧 L9: Accept optional custom rates from orchard settings
   * 🔧 L16: Accept optional unpaid break deduction (gross → net hours)
   */
  preparePayrollData(
    crew: Picker[],
    date: string = todayNZST(),
    options?: {
      pieceRate?: number;
      minWage?: number;
      unpaidBreakMinutes?: number;
    }
  ): PayrollExportData {
    const pieceRate = options?.pieceRate ?? PIECE_RATE;
    const minWage = options?.minWage ?? MINIMUM_WAGE;
    const unpaidBreakHours = (options?.unpaidBreakMinutes ?? 0) / 60;

    const crewData = crew.map(picker => {
      const grossHours = picker.hours || 0;
      // 🔧 L16: Deduct unpaid break if worker worked enough hours (NZ: 30min if >6h)
      const hours = grossHours > 6 ? Math.max(0, grossHours - unpaidBreakHours) : grossHours;
      const pieceEarnings = (picker.total_buckets_today || 0) * pieceRate;
      const minimumGuarantee = hours * minWage;
      const minimumTopUp = Math.max(0, minimumGuarantee - pieceEarnings);
      const totalEarnings = pieceEarnings + minimumTopUp;

      return {
        id: picker.id,
        name: picker.name,
        employeeId: picker.picker_id || 'N/A',
        buckets: picker.total_buckets_today || 0,
        hours,
        pieceEarnings,
        minimumTopUp,
        totalEarnings,
        status: picker.status,
      };
    });

    const summary = {
      totalBuckets: crewData.reduce((sum, p) => sum + p.buckets, 0),
      totalHours: crewData.reduce((sum, p) => sum + p.hours, 0),
      totalPieceEarnings: crewData.reduce((sum, p) => sum + p.pieceEarnings, 0),
      totalMinimumTopUp: crewData.reduce((sum, p) => sum + p.minimumTopUp, 0),
      grandTotal: crewData.reduce((sum, p) => sum + p.totalEarnings, 0),
      averageBucketsPerHour: 0,
    };

    summary.averageBucketsPerHour = summary.totalHours > 0
      ? Math.round((summary.totalBuckets / summary.totalHours) * 10) / 10
      : 0;

    return { date, crew: crewData, summary };
  },

  /**
   * Generate CSV content from payroll data
   */
  generateCSV(data: PayrollExportData): string {
    const headers = [
      'Employee ID',
      'Name',
      'Buckets',
      'Hours',
      'Piece Earnings (NZD)',
      'Minimum Top-Up (NZD)',
      'Total Earnings (NZD)',
      'Status',
    ];

    // 🔧 V2: All string cells sanitized against CSV injection
    const rows = data.crew.map(p => [
      escCsv(p.employeeId),
      escCsv(p.name),
      p.buckets.toString(),
      p.hours.toFixed(1),
      p.pieceEarnings.toFixed(2),
      p.minimumTopUp.toFixed(2),
      p.totalEarnings.toFixed(2),
      escCsv(p.status),
    ]);

    // Add summary rows
    rows.push([]);
    rows.push(['SUMMARY']);
    rows.push(['Total Buckets', '', data.summary.totalBuckets.toString()]);
    rows.push(['Total Hours', '', '', data.summary.totalHours.toFixed(1)]);
    rows.push(['Total Piece Earnings', '', '', '', data.summary.totalPieceEarnings.toFixed(2)]);
    rows.push(['Total Minimum Top-Up', '', '', '', '', data.summary.totalMinimumTopUp.toFixed(2)]);
    rows.push(['Grand Total', '', '', '', '', '', data.summary.grandTotal.toFixed(2)]);
    rows.push(['Avg Buckets/Hour', data.summary.averageBucketsPerHour.toString()]);

    const csvContent = [
      `Payroll Report - ${data.date}`,
      '',
      headers.join(','),
      ...rows.map(row => row.join(',')),
    ].join('\n');

    return csvContent;
  },

  /** Generate PDF-ready HTML content (delegated to export-pdf-template.service) */
  generatePDFContent,

  /**
   * Download file to user's device
   */
  downloadFile(content: string, filename: string, mimeType: string): void {
    const blob = new Blob([content], { type: mimeType });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
  },

  /**
   * Export payroll to CSV
   */
  exportToCSV(crew: Picker[], date?: string): void {
    const data = this.preparePayrollData(crew, date);
    const csv = this.generateCSV(data);
    const filename = `payroll-${data.date}.csv`;
    this.downloadFile(csv, filename, 'text/csv;charset=utf-8');
  },

  /**
   * Export payroll to PDF (opens print dialog)
   */
  exportToPDF(crew: Picker[], date?: string): void {
    const data = this.preparePayrollData(crew, date);
    const html = this.generatePDFContent(data);

    // Open in new window for printing
    const printWindow = window.open('', '_blank');
    if (printWindow) {
      printWindow.document.write(html);
      printWindow.document.close();
      printWindow.onload = () => {
        printWindow.print();
      };
    }
  },

  /**
   * Export payroll as a downloadable HTML report file.
   * Works offline, no external dependency needed.
   * Users can open in browser and print, or use "Save as PDF" in Chrome.
   */
  exportToPDFDownload(crew: Picker[], date?: string): void {
    const data = this.preparePayrollData(crew, date);
    const html = this.generatePDFContent(data);
    const filename = `payroll-report-${data.date}.html`;
    this.downloadFile(html, filename, 'text/html;charset=utf-8');
  },

  // =============================================
  // PAYROLL FORMAT ADAPTERS (delegated)
  // =============================================
  generateXeroCSV: generateXeroCSV,
  generatePaySauceCSV: generatePaySauceCSV,

  /** Export to Xero format and download */
  exportToXero(crew: Picker[], date?: string): void {
    const data = this.preparePayrollData(crew, date);
    const csv = generateXeroCSV(data);
    this.downloadFile(csv, `xero-payroll-${data.date}.csv`, 'text/csv;charset=utf-8');
  },

  /** Export to PaySauce format and download */
  exportToPaySauce(crew: Picker[], date?: string): void {
    const data = this.preparePayrollData(crew, date);
    const csv = generatePaySauceCSV(data);
    this.downloadFile(csv, `paysauce-payroll-${data.date}.csv`, 'text/csv;charset=utf-8');
  },
};

/** Available export format types */
export type ExportFormat = 'csv' | 'xero' | 'paysauce' | 'pdf';

export default exportService;

