/**
 * Export Modal Component — Enterprise Edition
 * Premium payroll export with format selection, live preview, and polished UX
 */
import { logger } from '@/utils/logger';
import React, { useState } from 'react';
import { Picker } from '../../types';
import { exportService, type ExportFormat } from '../../services/export.service';
import { todayNZST } from '@/utils/nzst';
import ModalOverlay from '@/components/ui/ModalOverlay';

interface ExportModalProps {
    crew: Picker[];
    onClose: () => void;
}

const FORMAT_OPTIONS: {
    id: ExportFormat;
    icon: string;
    label: string;
    desc: string;
    gradient: string;
}[] = [
        { id: 'csv', icon: 'table_chart', label: 'CSV', desc: 'Excel / Google Sheets', gradient: 'from-emerald-500 to-emerald-600' },
        { id: 'xero', icon: 'account_balance', label: 'Xero', desc: 'Direct payroll import', gradient: 'from-sky-500 to-blue-600' },
        { id: 'paysauce', icon: 'restaurant', label: 'PaySauce', desc: 'Bulk upload format', gradient: 'from-orange-500 to-amber-600' },
        { id: 'pdf', icon: 'picture_as_pdf', label: 'PDF', desc: 'Print-ready report', gradient: 'from-rose-500 to-red-600' },
    ];

const ExportModal: React.FC<ExportModalProps> = ({ crew, onClose }) => {
    const [format, setFormat] = useState<ExportFormat>('csv');
    const [isExporting, setIsExporting] = useState(false);
    const [exportSuccess, setExportSuccess] = useState(false);
    const [selectedDate, setSelectedDate] = useState(todayNZST());

    const handleExport = async () => {
        setIsExporting(true);
        try {
            switch (format) {
                case 'csv':
                    exportService.exportToCSV(crew, selectedDate);
                    break;
                case 'xero':
                    exportService.exportToXero(crew, selectedDate);
                    break;
                case 'paysauce':
                    exportService.exportToPaySauce(crew, selectedDate);
                    break;
                case 'pdf':
                    exportService.exportToPDF(crew, selectedDate);
                    break;
            }
            await new Promise(resolve => setTimeout(resolve, 600));
            setExportSuccess(true);
            setTimeout(() => onClose(), 1200);
        } catch (error) {
            logger.error('Export failed:', error);
        } finally {
            setIsExporting(false);
        }
    };

    // Calculate preview summary
    const summary = exportService.preparePayrollData(crew, selectedDate).summary;
    const selectedFormat = FORMAT_OPTIONS.find(f => f.id === format)!;

    return (
        <ModalOverlay onClose={onClose}>
            {/* ── Header ──────────────────── */}
            <div className={`bg-gradient-to-r ${selectedFormat.gradient} p-6 text-white relative overflow-hidden rounded-t-2xl`}>
                <div className="absolute -top-10 -right-10 w-32 h-32 bg-white/10 rounded-full blur-2xl" />
                <div className="relative z-10">
                    <div className="flex items-center justify-between mb-3">
                        <div className="flex items-center gap-3">
                            <div className="w-10 h-10 rounded-xl bg-white/20 backdrop-blur-sm flex items-center justify-center">
                                <span className="material-symbols-outlined text-xl">download</span>
                            </div>
                            <div>
                                <h2 className="text-xl font-black">Export Payroll</h2>
                                <p className="text-white/70 text-xs font-medium">{selectedDate}</p>
                            </div>
                        </div>
                        <button
                            onClick={onClose}
                            className="p-2 hover:bg-white/20 rounded-full transition-colors"
                        >
                            <span className="material-symbols-outlined">close</span>
                        </button>
                    </div>

                    {/* Mini Stats Bar */}
                    <div className="flex gap-4 mt-2">
                        <div className="flex items-center gap-1.5 text-white/80 text-xs font-medium">
                            <span className="material-symbols-outlined text-sm">groups</span>
                            {crew.length} pickers
                        </div>
                        <div className="flex items-center gap-1.5 text-white/80 text-xs font-medium">
                            <span className="material-symbols-outlined text-sm">shopping_basket</span>
                            {summary.totalBuckets} buckets
                        </div>
                        <div className="flex items-center gap-1.5 text-white font-bold text-sm">
                            ${summary.grandTotal.toFixed(2)}
                        </div>
                    </div>
                </div>
            </div>

            {/* ── Success State ────────────── */}
            {exportSuccess ? (
                <div className="p-10 text-center">
                    <div className="w-16 h-16 rounded-full bg-emerald-100 flex items-center justify-center mx-auto mb-4">
                        <span className="material-symbols-outlined text-emerald-600 text-3xl">check_circle</span>
                    </div>
                    <h3 className="text-lg font-bold text-text-main mb-1">Export Complete</h3>
                    <p className="text-text-muted text-sm">Your {selectedFormat.label} file has been downloaded.</p>
                </div>
            ) : (
                <>
                    {/* ── Content ─────────────── */}
                    <div className="p-5 space-y-5">
                        {/* Date Selection */}
                        <div>
                            <label className="block text-xs font-bold text-text-muted uppercase tracking-wider mb-2">
                                Report Date
                            </label>
                            <input
                                type="date"
                                value={selectedDate}
                                onChange={e => setSelectedDate(e.target.value)}
                                title="Report date"
                                className="w-full px-4 py-3 border-2 border-border-light rounded-xl focus:border-primary outline-none transition-colors font-medium text-text-main"
                            />
                        </div>

                        {/* Format Selection */}
                        <div>
                            <label className="block text-xs font-bold text-text-muted uppercase tracking-wider mb-2">
                                Export Format
                            </label>
                            <div className="grid grid-cols-2 gap-2.5">
                                {FORMAT_OPTIONS.map(opt => (
                                    <button
                                        key={opt.id}
                                        onClick={() => setFormat(opt.id)}
                                        className={`p-3.5 rounded-xl border-2 transition-all text-left group ${format === opt.id
                                            ? 'border-primary bg-primary/5 scale-[1.02]'
                                            : 'border-border-light hover:border-slate-300'
                                            }`}
                                    >
                                        <div className="flex items-center gap-2.5">
                                            <div className={`w-9 h-9 rounded-lg bg-gradient-to-br ${opt.gradient} flex items-center justify-center shadow-sm`}>
                                                <span className="material-symbols-outlined text-white text-lg">{opt.icon}</span>
                                            </div>
                                            <div>
                                                <p className="font-bold text-sm text-text-main">{opt.label}</p>
                                                <p className="text-[10px] text-text-muted font-medium">{opt.desc}</p>
                                            </div>
                                        </div>
                                        {format === opt.id && (
                                            <div className="mt-2 flex justify-end">
                                                <span className="material-symbols-outlined text-primary text-sm">check_circle</span>
                                            </div>
                                        )}
                                    </button>
                                ))}
                            </div>
                        </div>

                        {/* Preview Summary */}
                        <div className="bg-slate-50 rounded-xl p-4 border border-border-light">
                            <div className="flex items-center gap-2 mb-3">
                                <span className="material-symbols-outlined text-text-muted text-sm">preview</span>
                                <h3 className="text-xs font-bold text-text-muted uppercase tracking-wider">Preview</h3>
                            </div>
                            <div className="grid grid-cols-4 gap-2 text-center">
                                <div>
                                    <p className="text-lg font-black text-text-main">{crew.length}</p>
                                    <p className="text-[10px] text-text-muted font-medium">Pickers</p>
                                </div>
                                <div>
                                    <p className="text-lg font-black text-text-main">{summary.totalBuckets}</p>
                                    <p className="text-[10px] text-text-muted font-medium">Buckets</p>
                                </div>
                                <div>
                                    <p className="text-lg font-black text-text-main">{summary.totalHours.toFixed(0)}</p>
                                    <p className="text-[10px] text-text-muted font-medium">Hours</p>
                                </div>
                                <div>
                                    <p className="text-lg font-black text-primary">${summary.grandTotal.toFixed(0)}</p>
                                    <p className="text-[10px] text-text-muted font-medium">Total NZD</p>
                                </div>
                            </div>
                        </div>
                    </div>

                    {/* ── Actions ─────────────── */}
                    <div className="p-5 pt-0 flex gap-3">
                        <button
                            onClick={onClose}
                            className="flex-1 py-3.5 px-4 bg-slate-100 text-text-sub rounded-xl font-bold text-sm hover:bg-slate-200 transition-colors"
                        >
                            Cancel
                        </button>
                        <button
                            onClick={handleExport}
                            disabled={isExporting}
                            className={`flex-1 py-3.5 px-4 bg-gradient-to-r ${selectedFormat.gradient} text-white rounded-xl font-bold text-sm hover:scale-[1.02] active:scale-[0.98] transition-all disabled:opacity-50 flex items-center justify-center gap-2 shadow-lg`}
                        >
                            {isExporting ? (
                                <>
                                    <span className="w-4 h-4 border-2 border-white/30 border-t-white rounded-full animate-spin" />
                                    Exporting...
                                </>
                            ) : (
                                <>
                                    <span className="material-symbols-outlined text-lg">download</span>
                                    Export {selectedFormat.label}
                                </>
                            )}
                        </button>
                    </div>
                </>
            )}
        </ModalOverlay>
    );
};

export default ExportModal;
