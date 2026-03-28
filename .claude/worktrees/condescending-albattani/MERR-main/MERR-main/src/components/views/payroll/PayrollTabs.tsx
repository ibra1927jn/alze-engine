/**
 * Payroll tab sub-components
 *   SummaryCard, PayrollDashboard, TimesheetsTab, WageCalculatorTab, ExportTab
 */
import React, { useState, useEffect } from 'react';
import { TableVirtuoso } from 'react-virtuoso';
import { PickerBreakdown } from '@/services/payroll.service';
import { payrollService } from '@/services/payroll.service';
import { attendanceRepository } from '@/repositories/attendance.repository';
import { logger } from '@/utils/logger';

/* ── Summary Card ── */
export const SummaryCard: React.FC<{ icon: string; iconColor: string; label: string; value: string; highlight?: boolean }> = ({ icon, iconColor, label, value, highlight }) => (
    <div className={`rounded-xl p-4 shadow-sm border ${highlight ? 'bg-orange-50 border-orange-200' : 'bg-white border-border-light'}`}>
        <div className="flex items-center gap-2 mb-1.5">
            <span className={`material-symbols-outlined text-lg ${iconColor}`}>{icon}</span>
            <span className="text-xs text-text-secondary font-medium">{label}</span>
        </div>
        <p className={`text-2xl font-black ${highlight ? 'text-orange-700' : 'text-text-primary'}`}>{value}</p>
    </div>
);

/* ── Dashboard Tab ── */
export const PayrollDashboard: React.FC<{ pickers: PickerBreakdown[]; settings: { bucket_rate: number; min_wage_rate: number } }> = ({ pickers, settings }) => (
    <div className="space-y-4">
        <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
            <div className="flex items-center justify-between mb-3">
                <h3 className="font-bold text-text-primary">Picker Breakdown</h3>
                <div className="flex items-center gap-3 text-xs text-text-secondary">
                    <span>Rate: ${settings.bucket_rate}/bucket</span>
                    <span>Min: ${settings.min_wage_rate}/hr</span>
                </div>
            </div>

            {pickers.length === 0 ? (
                <div className="text-center py-8 text-text-muted">
                    <span className="material-symbols-outlined text-4xl mb-2 block">payments</span>
                    <p className="font-medium">No payroll data yet</p>
                    <p className="text-xs mt-1">Data will appear when scans are submitted</p>
                </div>
            ) : (
                <div className="overflow-x-auto dynamic-height" style={{ '--h': `${Math.min(pickers.length * 44 + 48, 600)}px` } as React.CSSProperties}>
                    <TableVirtuoso
                        data={pickers}
                        fixedHeaderContent={() => (
                            <tr className="border-b border-border-light text-xs text-text-secondary uppercase bg-white">
                                <th className="text-left py-2 font-medium">Worker</th>
                                <th className="text-right py-2 font-medium">Buckets</th>
                                <th className="text-right py-2 font-medium">Hours</th>
                                <th className="text-right py-2 font-medium">Piece Rate</th>
                                <th className="text-right py-2 font-medium">Top-Up</th>
                                <th className="text-right py-2 font-medium">Total</th>
                                <th className="text-center py-2 font-medium">Status</th>
                            </tr>
                        )}
                        itemContent={(_index, p) => (
                            <>
                                <td className="py-2.5 font-medium text-text-primary">{p.picker_name}</td>
                                <td className="py-2.5 text-right text-text-primary">{p.buckets}</td>
                                <td className="py-2.5 text-right text-text-primary">{p.hours_worked.toFixed(1)}</td>
                                <td className="py-2.5 text-right text-text-primary">${p.piece_rate_earnings.toFixed(0)}</td>
                                <td className={`py-2.5 text-right font-medium ${p.top_up_required > 0 ? 'text-amber-600' : 'text-text-muted'}`}>
                                    ${p.top_up_required.toFixed(0)}
                                </td>
                                <td className="py-2.5 text-right font-bold text-text-primary">${p.total_earnings.toFixed(0)}</td>
                                <td className="py-2.5 text-center">
                                    {p.is_below_minimum ? (
                                        <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-[10px] font-bold bg-amber-100 text-amber-700">
                                            <span className="material-symbols-outlined text-xs">shield</span> Shield
                                        </span>
                                    ) : (
                                        <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-[10px] font-bold bg-emerald-100 text-emerald-700">
                                            <span className="material-symbols-outlined text-xs">check</span> OK
                                        </span>
                                    )}
                                </td>
                            </>
                        )}
                    />
                </div>
            )}
        </div>
    </div>
);

/* ── Timesheets Tab ── */
export const TimesheetsTab: React.FC<{ orchardId?: string }> = ({ orchardId }) => {
    const [timesheets, setTimesheets] = useState<import('@/services/payroll.service').TimesheetEntry[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [toast, setToast] = useState<{ message: string; type: 'success' | 'error' } | null>(null);

    const showToast = (message: string, type: 'success' | 'error' = 'success') => {
        setToast({ message, type });
        setTimeout(() => setToast(null), 3000);
    };

    const loadTimesheets = async () => {
        if (!orchardId) { setIsLoading(false); return; }
        setIsLoading(true);
        const data = await payrollService.fetchTimesheets(orchardId);
        setTimesheets(data);
        setIsLoading(false);
    };

    // eslint-disable-next-line react-hooks/exhaustive-deps
    useEffect(() => { loadTimesheets(); }, [orchardId]);

    const handleApprove = async (id: string, name: string) => {
        const entry = timesheets.find(t => t.id === id);
        try {
            await payrollService.approveTimesheet(id, 'current_user', entry?.updated_at);
            setTimesheets(prev => prev.map(t => t.id === id ? { ...t, is_verified: true, verified_by: 'current_user' } : t));
            showToast(`Timesheet approved for ${name}`);
        } catch (err) {
            logger.error('[Payroll] Failed to approve timesheet:', err);
            showToast(`Failed to approve timesheet for ${name}`, 'error');
        }
    };

    const handleReject = async (id: string, name: string) => {
        try {
            await attendanceRepository.update(id, { rejected: true, rejected_at: new Date().toISOString() });
            setTimesheets(prev => prev.filter(t => t.id !== id));
            showToast(`Timesheet rejected for ${name}`, 'error');
        } catch (err) {
            logger.error('[Payroll] Failed to reject timesheet:', err);
            showToast(`Failed to reject timesheet for ${name}`, 'error');
        }
    };

    const pending = timesheets.filter(t => !t.is_verified);
    const approved = timesheets.filter(t => t.is_verified);

    return (
        <div className="space-y-4 relative">
            {toast && (
                <div className={`fixed top-4 right-4 z-50 px-4 py-3 rounded-xl shadow-lg text-sm font-medium flex items-center gap-2 ${toast.type === 'success' ? 'bg-emerald-600 text-white' : 'bg-red-600 text-white'}`}>
                    <span className="material-symbols-outlined text-sm">check_circle</span>
                    {toast.message}
                </div>
            )}

            <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
                <div className="flex items-center justify-between mb-4">
                    <h3 className="font-bold text-text-primary">Pending Approval</h3>
                    <span className="text-xs font-bold px-2 py-0.5 rounded-full bg-amber-100 text-amber-700">{pending.length}</span>
                </div>
                {isLoading ? (
                    <div className="text-center py-8 text-text-muted">
                        <div className="w-8 h-8 border-3 border-orange-200 border-t-orange-600 rounded-full animate-spin mx-auto mb-2" />
                        <p className="text-xs">Loading timesheets...</p>
                    </div>
                ) : pending.length === 0 ? (
                    <div className="text-center py-8 text-text-muted">
                        <span className="material-symbols-outlined text-4xl mb-2 block">task_alt</span>
                        <p className="font-medium">All timesheets approved</p>
                    </div>
                ) : (
                    <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
                        {pending.map(t => (
                            <div key={t.id} className="rounded-xl p-4 border border-border-light hover:shadow-sm transition-shadow">
                                <div className="flex items-center justify-between mb-2">
                                    <h4 className="font-bold text-text-primary text-sm">{t.picker_name}</h4>
                                    <span className="text-xs text-text-secondary">{t.hours_worked.toFixed(1)}h</span>
                                </div>
                                <div className="flex items-center gap-3 text-xs text-text-secondary mb-3">
                                    <span>{t.check_in_time ? new Date(t.check_in_time).toLocaleTimeString('en-NZ', { hour: '2-digit', minute: '2-digit' }) : '—'}</span>
                                    <span>→</span>
                                    <span>{t.check_out_time ? new Date(t.check_out_time).toLocaleTimeString('en-NZ', { hour: '2-digit', minute: '2-digit' }) : 'Active'}</span>
                                </div>
                                <div className="flex gap-2">
                                    <button onClick={() => handleApprove(t.id, t.picker_name)} className="flex-1 py-1.5 rounded-lg bg-emerald-50 text-emerald-700 text-xs font-bold hover:bg-emerald-100 transition-colors flex items-center justify-center gap-1">
                                        <span className="material-symbols-outlined text-xs">check</span>Approve
                                    </button>
                                    <button onClick={() => handleReject(t.id, t.picker_name)} className="flex-1 py-1.5 rounded-lg bg-red-50 text-red-700 text-xs font-bold hover:bg-red-100 transition-colors flex items-center justify-center gap-1">
                                        <span className="material-symbols-outlined text-xs">close</span>Reject
                                    </button>
                                </div>
                            </div>
                        ))}
                    </div>
                )}
            </div>

            {approved.length > 0 && (
                <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
                    <h3 className="font-bold text-text-primary mb-4">Approved ({approved.length})</h3>
                    <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
                        {approved.map(t => (
                            <div key={t.id} className="rounded-xl p-4 border border-emerald-100 bg-emerald-50/30">
                                <div className="flex items-center justify-between mb-1">
                                    <h4 className="font-bold text-text-primary text-sm">{t.picker_name}</h4>
                                    <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-[10px] font-bold bg-emerald-100 text-emerald-700">
                                        <span className="material-symbols-outlined text-xs">check</span> Verified
                                    </span>
                                </div>
                                <div className="flex items-center gap-3 text-xs text-text-secondary">
                                    <span>{t.hours_worked.toFixed(1)}h</span>
                                    <span>•</span>
                                    <span>
                                        {t.check_in_time ? new Date(t.check_in_time).toLocaleTimeString('en-NZ', { hour: '2-digit', minute: '2-digit' }) : '—'} → {t.check_out_time ? new Date(t.check_out_time).toLocaleTimeString('en-NZ', { hour: '2-digit', minute: '2-digit' }) : 'Active'}
                                    </span>
                                </div>
                            </div>
                        ))}
                    </div>
                </div>
            )}
        </div>
    );
};

/* ── Wage Calculator Tab ── */
export const WageCalculatorTab: React.FC<{ settings: { bucket_rate: number; min_wage_rate: number } }> = ({ settings }) => {
    const [buckets, setBuckets] = useState(0);
    const [hours, setHours] = useState(8);

    const pieceRate = buckets * settings.bucket_rate;
    const minimumRequired = hours * settings.min_wage_rate;
    const topUp = Math.max(0, minimumRequired - pieceRate);
    const total = pieceRate + topUp;

    return (
        <div className="max-w-lg space-y-4">
            <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
                <h3 className="font-bold text-text-primary mb-4">Quick Wage Calculator</h3>
                <div className="space-y-4">
                    <div>
                        <label htmlFor="wage-calc-buckets" className="block text-xs text-text-secondary font-medium mb-1">Buckets Picked</label>
                        <input id="wage-calc-buckets" type="number" value={buckets} onChange={e => setBuckets(Number(e.target.value))} className="w-full px-4 py-2.5 rounded-xl border border-border-light text-text-primary font-medium focus:border-orange-500 focus:ring-2 focus:ring-orange-500/10 outline-none" />
                    </div>
                    <div>
                        <label htmlFor="wage-calc-hours" className="block text-xs text-text-secondary font-medium mb-1">Hours Worked</label>
                        <input id="wage-calc-hours" type="number" step="0.5" value={hours} onChange={e => setHours(Number(e.target.value))} className="w-full px-4 py-2.5 rounded-xl border border-border-light text-text-primary font-medium focus:border-orange-500 focus:ring-2 focus:ring-orange-500/10 outline-none" />
                    </div>
                </div>
            </div>

            <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
                <h3 className="font-bold text-text-primary mb-3">Result</h3>
                <div className="space-y-2">
                    <div className="flex justify-between py-1.5">
                        <span className="text-sm text-text-secondary">Piece Rate ({buckets} × ${settings.bucket_rate})</span>
                        <span className="text-sm font-bold text-text-primary">${pieceRate.toFixed(2)}</span>
                    </div>
                    <div className="flex justify-between py-1.5">
                        <span className="text-sm text-text-secondary">Minimum Required ({hours}h × ${settings.min_wage_rate})</span>
                        <span className="text-sm font-bold text-text-primary">${minimumRequired.toFixed(2)}</span>
                    </div>
                    {topUp > 0 && (
                        <div className="flex justify-between py-1.5 text-amber-600">
                            <span className="text-sm flex items-center gap-1">
                                <span className="material-symbols-outlined text-xs">shield</span>Wage Shield Top-Up
                            </span>
                            <span className="text-sm font-bold">+${topUp.toFixed(2)}</span>
                        </div>
                    )}
                    <div className="border-t border-border-light pt-2 mt-2 flex justify-between">
                        <span className="text-sm font-bold text-text-primary">Total Earnings</span>
                        <span className="text-lg font-black text-orange-600">${total.toFixed(2)}</span>
                    </div>
                </div>
            </div>
        </div>
    );
};

/* ── Export Tab ── */
export const ExportTab: React.FC = () => (
    <div className="space-y-4 max-w-lg">
        <div className="bg-white rounded-xl p-8 shadow-sm border border-border-light text-center">
            <span className="material-symbols-outlined text-text-disabled text-5xl mb-3 block">download</span>
            <h3 className="font-bold text-text-primary mb-1">Export Payroll Data</h3>
            <p className="text-sm text-text-secondary mb-4">Export payroll data for accounting software integration</p>
        </div>
        <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
            {[
                { label: 'CSV Export', desc: 'Standard spreadsheet format', icon: 'table_chart', format: 'csv' },
                { label: 'Xero Integration', desc: 'Coming soon', icon: 'cloud_upload', format: 'xero' },
                { label: 'PaySauce', desc: 'Coming soon', icon: 'cloud_upload', format: 'paysauce' },
                { label: 'PDF Report', desc: 'Printable summary', icon: 'picture_as_pdf', format: 'pdf' },
            ].map(exp => (
                <button key={exp.format} disabled={exp.desc === 'Coming soon'} className="bg-white rounded-xl p-4 shadow-sm border border-border-light flex items-center gap-3 hover:shadow-md transition-shadow text-left disabled:opacity-40 disabled:cursor-not-allowed">
                    <div className="size-10 rounded-lg bg-orange-50 flex items-center justify-center">
                        <span className="material-symbols-outlined text-orange-600">{exp.icon}</span>
                    </div>
                    <div>
                        <h4 className="font-bold text-text-primary text-sm">{exp.label}</h4>
                        <p className="text-xs text-text-secondary">{exp.desc}</p>
                    </div>
                </button>
            ))}
        </div>
    </div>
);
