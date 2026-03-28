/**
 * WeeklyReportView — Manager Weekly Summary Report
 *
 * Refactored architecture:
 *   WeeklyReportView.tsx  — Thin orchestrator (~100 lines)
 *   useWeeklyReport.ts   — Data hook (loading, calculations, state)
 *   weeklyReportExport.ts — PDF/CSV export utilities
 *   weekly-report/
 *     ├── KpiCards.tsx        — Summary metric cards
 *     ├── TrendChartCard.tsx  — Chart wrapper
 *     ├── DayDetailPanel.tsx  — Day drill-down
 *     ├── ExportModal.tsx     — Export format picker
 *     └── index.ts            — Barrel export
 */
import React from 'react';
import { useWeeklyReport } from '@/hooks/useWeeklyReport';
import { exportCSV, exportPDF } from '@/utils/weeklyReportExport';
import { TrendDataPoint } from '@/components/charts/TrendLineChart';
import LoadingSkeleton from '@/components/ui/LoadingSkeleton';
import { KpiCards, TrendChartCard, DayDetailPanel, ExportModal } from './weekly-report';

const WeeklyReportView: React.FC = () => {
    const report = useWeeklyReport();

    const handlePointClick = (point: TrendDataPoint, _index: number) => {
        if (point.meta) {
            report.setSelectedDayMeta(prev =>
                prev?.date === point.meta?.date ? null : point.meta!
            );
        }
    };

    const exportContext = {
        pickers: report.pickers,
        binsTrend: report.binsTrend,
        workforceTrend: report.workforceTrend,
        teamRankings: report.teamRankings,
        crew: report.crew,
        orchardName: report.orchard?.name || 'Orchard',
        totalBuckets: report.totalBuckets,
        totalHours: report.totalHours,
        totalEarnings: report.totalEarnings,
        avgBPA: report.avgBPA,
        costPerBin: report.costPerBin,
        exportSections: { summary: true, charts: true, teams: true, pickerDetail: true },
    };

    const kpiCards = [
        { icon: 'inventory_2', label: 'Total Bins', value: report.totalBuckets.toString(), gradient: 'from-sky-50 to-blue-50', iconBg: 'bg-sky-100 text-sky-600' },
        { icon: 'schedule', label: 'Total Hours', value: `${report.totalHours.toFixed(0)}h`, gradient: 'from-amber-50 to-orange-50', iconBg: 'bg-amber-100 text-amber-600' },
        { icon: 'payments', label: 'Total Labour', value: `$${report.totalEarnings.toFixed(0)}`, gradient: 'from-emerald-50 to-teal-50', iconBg: 'bg-emerald-100 text-emerald-600' },
        { icon: 'speed', label: 'Avg Bins/Hr', value: report.avgBPA.toFixed(1), gradient: 'from-purple-50 to-violet-50', iconBg: 'bg-purple-100 text-purple-600' },
        { icon: 'attach_money', label: 'Cost/Bin', value: `$${report.costPerBin.toFixed(2)}`, gradient: 'from-rose-50 to-pink-50', iconBg: 'bg-rose-100 text-rose-600' },
    ];

    if (report.isLoading) {
        return (
            <div className="space-y-5 max-w-5xl mx-auto">
                <div className="grid grid-cols-2 lg:grid-cols-5 gap-3"><LoadingSkeleton type="metric" count={5} /></div>
                <LoadingSkeleton type="card" count={2} />
            </div>
        );
    }

    return (
        <div className="space-y-5 max-w-5xl mx-auto">
            {/* Header */}
            <div className="flex items-center justify-between">
                <div>
                    <h2 className="text-xl font-black text-text-main">Weekly Report</h2>
                    <p className="text-xs text-text-muted">
                        {report.orchard?.name || 'Orchard'} — {new Date().toLocaleDateString('en-NZ', { weekday: 'long', month: 'long', day: 'numeric' })}
                    </p>
                </div>
                <button
                    onClick={() => report.setShowExportModal(true)}
                    className="flex items-center gap-2 px-5 py-2.5 rounded-xl gradient-primary glow-primary text-white font-bold text-sm hover:scale-105 transition-all active:scale-95 shadow-lg"
                >
                    <span className="material-symbols-outlined text-lg">download</span> Export Report
                </button>
            </div>

            <KpiCards cards={kpiCards} />

            {/* Trend Charts */}
            <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
                <TrendChartCard
                    title="Harvest Velocity" subtitle="Daily bins produced — 7 day trend"
                    icon="show_chart" colorTheme="emerald"
                    iconBgClass="bg-emerald-50" iconTextClass="text-emerald-500" bgIconClass="text-emerald-400"
                    data={report.binsTrend} targetLine={report.dailyBinTarget} targetLabel="Daily Target"
                    valueSuffix=" bins" staggerClass="stagger-3" onPointClick={handlePointClick}
                />
                <TrendChartCard
                    title="Workforce Size" subtitle="Active pickers per day"
                    icon="group" colorTheme="blue"
                    iconBgClass="bg-blue-50" iconTextClass="text-blue-500" bgIconClass="text-blue-300"
                    data={report.workforceTrend}
                    valueSuffix=" pickers" staggerClass="stagger-4" onPointClick={handlePointClick}
                />
            </div>

            {report.selectedDayMeta && (
                <DayDetailPanel meta={report.selectedDayMeta} onClose={() => report.setSelectedDayMeta(null)} />
            )}

            {/* Team Rankings */}
            <div className="bg-white rounded-2xl p-5 shadow-lg shadow-slate-200/50 border border-slate-100 dash-card-enter anim-delay" style={{ '--delay': '250ms' } as React.CSSProperties}>
                <h3 className="font-bold text-text-main mb-4 flex items-center gap-2">
                    <span className="material-symbols-outlined text-amber-500">emoji_events</span> Team Rankings
                </h3>
                {report.teamRankings.length === 0 ? (
                    <div className="flex flex-col items-center justify-center py-8">
                        <div className="w-16 h-16 rounded-full bg-slate-100 flex items-center justify-center mb-3">
                            <span className="material-symbols-outlined text-3xl text-slate-300">leaderboard</span>
                        </div>
                        <p className="text-sm font-bold text-text-sub">No team data</p>
                        <p className="text-xs text-text-muted mt-1">Rankings appear as data flows in</p>
                    </div>
                ) : (
                    <div className="space-y-2">
                        {report.teamRankings.map((team, i) => (
                            <div key={team.name} className="flex items-center gap-3 p-3 rounded-xl bg-slate-50 hover:bg-slate-100 hover:shadow-sm transition-all">
                                <span className={`w-8 h-8 rounded-lg flex items-center justify-center font-black text-sm shadow-sm ${i === 0 ? 'bg-gradient-to-br from-amber-400 to-amber-500 text-white' : i === 1 ? 'bg-gradient-to-br from-slate-300 to-slate-400 text-white' : i === 2 ? 'bg-gradient-to-br from-orange-400 to-orange-500 text-white' : 'bg-slate-100 text-text-muted'}`}>
                                    {i + 1}
                                </span>
                                <div className="flex-1">
                                    <span className="text-sm font-bold text-text-main">{team.name}</span>
                                    <span className="text-xs text-text-muted ml-2">({team.count} pickers)</span>
                                </div>
                                <div className="text-right">
                                    <p className="text-sm font-bold text-text-main">{team.bpa.toFixed(1)} bins/hr</p>
                                    <p className="text-[10px] text-text-muted">{team.buckets} bins • ${team.earnings.toFixed(0)}</p>
                                </div>
                            </div>
                        ))}
                    </div>
                )}
            </div>

            {/* Top 10 Pickers */}
            <div className="bg-white rounded-2xl p-5 shadow-lg shadow-slate-200/50 border border-slate-100 dash-card-enter" style={{ animationDelay: '350ms' }}>
                <h3 className="font-bold text-text-main mb-4 flex items-center gap-2">
                    <span className="material-symbols-outlined text-emerald-500">star</span> Top 10 Pickers
                </h3>
                {report.pickers.length === 0 ? (
                    <div className="flex flex-col items-center justify-center py-8">
                        <div className="w-16 h-16 rounded-full bg-slate-100 flex items-center justify-center mb-3">
                            <span className="material-symbols-outlined text-3xl text-slate-300">person_search</span>
                        </div>
                        <p className="text-sm font-bold text-text-sub">No pickers yet</p>
                        <p className="text-xs text-text-muted mt-1">Picker stats appear as scans are submitted</p>
                    </div>
                ) : (
                    <div className="space-y-1">
                        {[...report.pickers].sort((a, b) => b.buckets - a.buckets).slice(0, 10).map((p, i) => (
                            <div key={p.picker_id} onClick={() => report.openProfile(p.picker_id)} className="flex items-center gap-3 py-2.5 px-2 -mx-2 border-b border-border-light last:border-0 hover:bg-slate-50 rounded-lg transition-colors cursor-pointer">
                                <span className={`w-6 h-6 rounded-lg flex items-center justify-center text-[10px] font-black shadow-sm ${i === 0 ? 'bg-amber-500 text-white' : i === 1 ? 'bg-slate-400 text-white' : i === 2 ? 'bg-orange-500 text-white' : 'bg-slate-100 text-text-muted'}`}>{i + 1}</span>
                                <span className="flex-1 text-sm font-medium text-text-main hover:text-indigo-600 transition-colors">{p.picker_name}</span>
                                <span className="text-xs text-sky-600 font-bold bg-sky-50 px-2 py-0.5 rounded-full">{p.buckets} bins</span>
                                <span className="text-xs text-text-muted">{p.hours_worked.toFixed(1)}h</span>
                                <span className="text-xs text-emerald-600 font-bold">${p.total_earnings.toFixed(0)}</span>
                            </div>
                        ))}
                    </div>
                )}
            </div>

            {report.showExportModal && (
                <ExportModal
                    onClose={() => report.setShowExportModal(false)}
                    onExportPDF={() => { exportPDF(exportContext); report.setShowExportModal(false); }}
                    onExportCSV={() => { exportCSV(exportContext); report.setShowExportModal(false); }}
                />
            )}
        </div>
    );
};

export default WeeklyReportView;
