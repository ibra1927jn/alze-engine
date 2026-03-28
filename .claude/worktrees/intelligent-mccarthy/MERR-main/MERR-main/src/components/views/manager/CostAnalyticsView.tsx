/**
 * CostAnalyticsView — Manager Cost Analytics Dashboard
 *
 * Refactored architecture:
 *   CostAnalyticsView.tsx     — Thin orchestrator (~180 lines)
 *   useCostAnalytics.ts       — Data hook (loading, calculations)
 *   cost-analytics/
 *     ├── CostCharts.tsx      — DonutChart, HBar, KPICard
 *     └── index.ts            — Barrel export
 *   weekly-report/
 *     └── DayDetailPanel.tsx  — Reused for day drill-down
 */
import React from 'react';
import { useCostAnalytics } from '@/hooks/useCostAnalytics';
import { TrendLineChart } from '@/components/charts/TrendLineChart';
import { DayDetailPanel } from './weekly-report';
import LoadingSkeleton from '@/components/ui/LoadingSkeleton';
import { DonutChart, HBar, KPICard } from './cost-analytics';

const CostAnalyticsView: React.FC = () => {
    const ca = useCostAnalytics();

    if (ca.isLoading) {
        return (
            <div className="space-y-5 max-w-6xl mx-auto">
                <div className="grid grid-cols-2 lg:grid-cols-4 gap-3"><LoadingSkeleton type="metric" count={4} /></div>
                <LoadingSkeleton type="card" count={2} />
            </div>
        );
    }

    return (
        <div className="space-y-5 max-w-6xl mx-auto">
            {/* KPI Summary */}
            <div className="grid grid-cols-2 lg:grid-cols-4 gap-3">
                <KPICard icon="payments" label="Cost/Bin" value={`$${ca.costPerBin.toFixed(2)}`} gradient="bg-gradient-to-br from-emerald-50 to-teal-50" iconColor="text-emerald-600" delay={0} />
                <KPICard icon="inventory_2" label="Total Bins" value={ca.totalBuckets.toString()} gradient="bg-gradient-to-br from-sky-50 to-blue-50" iconColor="text-sky-600" delay={50} />
                <KPICard icon="account_balance_wallet" label="Total Labour" value={`$${ca.totalEarnings.toFixed(0)}`} gradient="bg-gradient-to-br from-amber-50 to-orange-50" iconColor="text-amber-600" delay={100} />
                <KPICard icon="trending_up" label="Min Wage Top-Up" value={`$${ca.totalTopUp.toFixed(0)}`} gradient="bg-gradient-to-br from-rose-50 to-pink-50" iconColor="text-rose-600" delay={150} />
            </div>

            {/* Cost Breakdown with Donut */}
            <div className="bg-white rounded-2xl p-5 shadow-lg shadow-slate-200/50 border border-slate-100 dash-card-enter anim-delay" style={{ '--delay': '200ms' } as React.CSSProperties}>
                <h3 className="font-bold text-text-main mb-1 flex items-center gap-2">
                    <span className="material-symbols-outlined text-indigo-500">donut_large</span>Cost Breakdown
                </h3>
                <p className="text-xs text-text-muted mb-4">Piece rate vs minimum wage top-up</p>
                <div className="grid grid-cols-1 md:grid-cols-3 gap-6 items-center">
                    <div className="md:col-span-1"><DonutChart pieceRate={ca.totalPieceRate} topUp={ca.totalTopUp} /></div>
                    <div className="md:col-span-2 space-y-4">
                        {[
                            { label: 'Piece Rate Earnings', desc: 'Performance-based pay', value: ca.totalPieceRate, color: 'emerald' },
                            { label: 'Minimum Wage Top-Up', desc: 'Legal compliance cost', value: ca.totalTopUp, color: 'amber' },
                        ].map(item => (
                            <div key={item.label} className={`flex items-center gap-3 p-3 rounded-xl bg-${item.color}-50/50`}>
                                <div className={`w-3 h-3 rounded-full bg-gradient-to-r from-${item.color}-500 to-${item.color}-400`} />
                                <div className="flex-1">
                                    <p className="text-sm font-bold text-text-main">{item.label}</p>
                                    <p className="text-xs text-text-muted">{item.desc}</p>
                                </div>
                                <span className={`text-lg font-black text-${item.color}-600`}>${item.value.toFixed(0)}</span>
                            </div>
                        ))}
                    </div>
                </div>
            </div>

            {/* Daily Cost Trend */}
            <div className="glass-card card-hover p-5 relative overflow-hidden group section-enter stagger-3">
                <div className="absolute right-0 top-0 p-4 opacity-5 group-hover:opacity-10 transition-opacity duration-500">
                    <span className="material-symbols-outlined text-7xl text-rose-400">trending_up</span>
                </div>
                <div className="relative z-10">
                    <h3 className="font-bold text-text-main mb-1 flex items-center gap-2">
                        <div className="w-8 h-8 rounded-lg bg-rose-50 flex items-center justify-center shadow-sm">
                            <span className="material-symbols-outlined text-base text-rose-500">trending_up</span>
                        </div>
                        Cost Per Bin — 7 Day Trend
                    </h3>
                    <p className="text-xs text-text-muted mb-3 ml-10">Red dots = above break-even threshold</p>
                    <TrendLineChart
                        data={ca.costTrend}
                        targetLine={ca.breakEven}
                        targetLabel="Break-even"
                        colorTheme="rose"
                        valuePrefix="$"
                        higherIsBetter={false}
                        height={220}
                        onPointClick={(point) => {
                            if (point.meta) {
                                ca.setSelectedDayMeta(prev => prev?.date === point.meta?.date ? null : point.meta!);
                            }
                        }}
                    />
                </div>
                {ca.selectedDayMeta && (
                    <div className="relative z-10 mt-4">
                        <DayDetailPanel meta={ca.selectedDayMeta} onClose={() => ca.setSelectedDayMeta(null)} />
                    </div>
                )}
            </div>

            {/* Cost Per Team */}
            <div className="bg-white rounded-2xl p-5 shadow-lg shadow-slate-200/50 border border-slate-100 dash-card-enter anim-delay" style={{ '--delay': '300ms' } as React.CSSProperties}>
                <h3 className="font-bold text-text-main mb-1 flex items-center gap-2">
                    <span className="material-symbols-outlined text-purple-500">groups</span>Cost Per Team
                </h3>
                <p className="text-xs text-text-muted mb-4">Lower cost/bin = more efficient</p>
                {ca.teamCosts.length === 0 ? (
                    <div className="flex flex-col items-center justify-center py-8 bg-gradient-to-b from-slate-50/50 to-white rounded-xl">
                        <div className="w-16 h-16 rounded-full bg-gradient-to-br from-indigo-100 to-purple-100 flex items-center justify-center mb-3 shadow-sm">
                            <span className="material-symbols-outlined text-2xl text-indigo-400">analytics</span>
                        </div>
                        <p className="text-sm font-bold text-text-sub">No team data yet</p>
                        <p className="text-xs text-text-muted mt-1">Data appears as pickers submit scans</p>
                    </div>
                ) : ca.teamCosts.map(team => (
                    <HBar key={team.teamLeader} label={team.teamLeader} value={team.costPerBin} max={ca.maxCostPerBin} color="bg-gradient-to-r from-indigo-500 to-purple-500" suffix="/bin" />
                ))}
            </div>

            {/* Top/Bottom Performers */}
            <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
                {[
                    { title: 'Most Efficient', subtitle: 'Lowest cost per bin', icon: 'emoji_events', iconColor: 'text-emerald-500', hoverBg: 'hover:bg-emerald-50/30', badgeBg: 'bg-emerald-50', badgeText: 'text-emerald-600', topBg: 'bg-emerald-500', items: ca.sortedByEfficiency.slice(0, 5), delay: 400 },
                    { title: 'Least Efficient', subtitle: 'Highest cost per bin', icon: 'warning', iconColor: 'text-amber-500', hoverBg: 'hover:bg-amber-50/30', badgeBg: 'bg-amber-50', badgeText: 'text-amber-600', topBg: 'bg-amber-500', items: ca.sortedByEfficiency.slice(-5).reverse(), delay: 450 },
                ].map(panel => (
                    <div key={panel.title} className="bg-white rounded-2xl p-5 shadow-lg shadow-slate-200/50 border border-slate-100 dash-card-enter anim-delay" style={{ '--delay': `${panel.delay}ms` } as React.CSSProperties}>
                        <h3 className="font-bold text-text-main mb-1 flex items-center gap-2">
                            <span className={`material-symbols-outlined ${panel.iconColor}`}>{panel.icon}</span>{panel.title}
                        </h3>
                        <p className="text-xs text-text-muted mb-3">{panel.subtitle}</p>
                        {panel.items.length === 0 ? (
                            <div className="flex flex-col items-center justify-center py-6">
                                <span className={`material-symbols-outlined text-2xl ${panel.iconColor} opacity-30 mb-1`}>{panel.icon}</span>
                                <p className="text-sm text-text-muted">Awaiting harvest data</p>
                            </div>
                        ) : panel.items.map((p, i) => (
                            <div key={p.picker_id} onClick={() => ca.openProfile(p.picker_id)} className={`flex items-center justify-between py-2 border-b border-border-light last:border-0 ${panel.hoverBg} transition-colors rounded-lg px-2 -mx-2 cursor-pointer`}>
                                <div className="flex items-center gap-2">
                                    <span className={`w-6 h-6 rounded-lg flex items-center justify-center text-[10px] font-black ${i === 0 ? `${panel.topBg} text-white` : 'bg-slate-100 text-text-muted'}`}>{i + 1}</span>
                                    <span className="text-sm font-medium text-text-main hover:text-indigo-600 transition-colors">{p.picker_name}</span>
                                </div>
                                <span className={`text-xs font-bold ${panel.badgeText} ${panel.badgeBg} px-2 py-0.5 rounded-full`}>${(p.total_earnings / p.buckets).toFixed(2)}/bin</span>
                            </div>
                        ))}
                    </div>
                ))}
            </div>
        </div>
    );
};

export default CostAnalyticsView;
