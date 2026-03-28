/**
 * AnomalyDetectionView — Intelligent Fraud Shield (Phase 7 v3)
 *
 * Now fetches anomalies from the backend Edge Function (detect-anomalies).
 * Falls back to mock data when offline or Edge Function unavailable.
 *
 * Smart rules implemented server-side:
 * 1. Elapsed-time velocity (not burst — accumulated buckets are normal)
 * 2. Peer comparison within same row (if everyone is fast = good tree)
 * 3. Grace period for shift warmup (first 90 min = no velocity alerts)
 *
 * Includes a "Smart Dismissals" section showing what the system ignored,
 * building manager trust that the system understands real orchard workflows.
 */
import React, { useState, useEffect, useCallback } from 'react';
import { fraudDetectionService, Anomaly, AnomalyType } from '../../../services/fraud-detection.service';
import { useHarvestStore } from '../../../stores/useHarvestStore';
import ComponentErrorBoundary from '../@/components/ui/ComponentErrorBoundary';

type FilterType = 'all' | AnomalyType;

const ANOMALY_CONFIG: Record<AnomalyType, { icon: string; color: string; bg: string; label: string }> = {
    impossible_velocity: { icon: 'speed', color: 'text-rose-500', bg: 'bg-rose-50', label: 'Impossible Rate' },
    post_collection_spike: { icon: 'electric_bolt', color: 'text-red-500', bg: 'bg-red-50', label: 'Post-Pickup Spike' },
    peer_outlier: { icon: 'group_off', color: 'text-amber-500', bg: 'bg-amber-50', label: 'Peer Outlier' },
    off_hours: { icon: 'schedule', color: 'text-blue-500', bg: 'bg-blue-50', label: 'Off Hours' },
    duplicate_proximity: { icon: 'content_copy', color: 'text-purple-500', bg: 'bg-purple-50', label: 'Duplicate Scan' },
};

const SEVERITY_STYLES: Record<string, string> = {
    high: 'bg-rose-100 text-rose-700 border-rose-200',
    medium: 'bg-amber-100 text-amber-700 border-amber-200',
    low: 'bg-sky-100 text-sky-700 border-sky-200',
};

const FILTER_LABELS: Record<FilterType, string> = {
    all: 'All Flags',
    impossible_velocity: 'Impossible Rate',
    post_collection_spike: 'Post-Pickup',
    peer_outlier: 'Peer Outlier',
    off_hours: 'Off Hours',
    duplicate_proximity: 'Duplicates',
};

const RULE_BADGE: Record<string, { label: string; color: string }> = {
    elapsed_velocity: { label: '⏱ Elapsed Time', color: 'text-rose-600 bg-rose-50' },
    peer_comparison: { label: '👥 Peer Check', color: 'text-amber-600 bg-amber-50' },
    off_hours: { label: '🌙 Off Hours', color: 'text-blue-600 bg-blue-50' },
    duplicate: { label: '📋 Duplicate', color: 'text-purple-600 bg-purple-50' },
    grace_period_exempt: { label: '🌅 Grace Period', color: 'text-emerald-600 bg-emerald-50' },
};

const AnomalyDetectionView: React.FC = () => {
    const [anomalies, setAnomalies] = useState<Anomaly[]>([]);
    const [loading, setLoading] = useState(true);
    const [isLive, setIsLive] = useState(false);
    const [filter, setFilter] = useState<FilterType>('all');
    const [showDismissed, setShowDismissed] = useState(false);
    const openPickerProfile = useHarvestStore(state => state.openPickerProfile);
    const orchard = useHarvestStore(state => state.orchard);
    const dismissed = fraudDetectionService.getDismissedExamples();

    const loadAnomalies = useCallback(async () => {
        setLoading(true);
        try {
            if (orchard?.id) {
                const result = await fraudDetectionService.fetchAnomalies(orchard.id);
                setAnomalies(result);
                setIsLive(true);
            } else {
                // No orchard — use mock data for demo
                setAnomalies(fraudDetectionService.getMockAnomalies());
                setIsLive(false);
            }
        } catch {
            setAnomalies(fraudDetectionService.getMockAnomalies());
            setIsLive(false);
        } finally {
            setLoading(false);
        }
    }, [orchard?.id]);

    useEffect(() => {
        loadAnomalies();
    }, [loadAnomalies]);

    const filtered = filter === 'all' ? anomalies : anomalies.filter(a => a.type === filter);
    const highCount = anomalies.filter(a => a.severity === 'high').length;
    const medCount = anomalies.filter(a => a.severity === 'medium').length;
    const lowCount = anomalies.filter(a => a.severity === 'low').length;

    return (
        <ComponentErrorBoundary componentName="Fraud Detection">
            <div className="space-y-6 animate-fade-in pb-20">
                {/* ─── Header Banner ─── */}
                <div className="bg-gradient-to-r from-slate-900 to-slate-800 rounded-2xl p-6 text-white shadow-xl relative overflow-hidden">
                    <span className="material-symbols-outlined absolute -right-8 -top-8 text-[200px] text-white/5 pointer-events-none select-none">
                        shield
                    </span>

                    <div className="flex flex-col md:flex-row justify-between md:items-center gap-4 relative z-10">
                        <div>
                            <h2 className="text-2xl font-bold flex items-center gap-3 mb-1">
                                <div className="p-2 bg-rose-500/20 rounded-lg border border-rose-500/30">
                                    <span className="material-symbols-outlined text-xl text-rose-400">shield</span>
                                </div>
                                Fraud Shield
                                {isLive && (
                                    <span className="ml-2 px-2 py-0.5 bg-emerald-500/20 border border-emerald-500/30 rounded-full text-xs font-semibold text-emerald-400 flex items-center gap-1">
                                        <span className="w-1.5 h-1.5 bg-emerald-400 rounded-full animate-pulse" />
                                        Live
                                    </span>
                                )}
                                {!isLive && !loading && (
                                    <span className="ml-2 px-2 py-0.5 bg-amber-500/20 border border-amber-500/30 rounded-full text-xs font-semibold text-amber-400">
                                        Demo
                                    </span>
                                )}
                            </h2>
                            <p className="text-slate-400 text-sm">
                                {isLive
                                    ? 'Server-side detection — real-time analysis of scan patterns'
                                    : 'Intelligent detection — understands real orchard workflows'}
                            </p>
                        </div>

                        <div className="flex gap-3">
                            <div className="text-center bg-slate-800/50 px-4 py-2 rounded-xl border border-slate-700">
                                <p className="text-xs font-bold uppercase tracking-wider text-slate-400">High</p>
                                <p className="text-2xl font-black text-rose-400 tabular-nums">{highCount}</p>
                            </div>
                            <div className="text-center bg-slate-800/50 px-4 py-2 rounded-xl border border-slate-700">
                                <p className="text-xs font-bold uppercase tracking-wider text-slate-400">Medium</p>
                                <p className="text-2xl font-black text-amber-400 tabular-nums">{medCount}</p>
                            </div>
                            <div className="text-center bg-slate-800/50 px-4 py-2 rounded-xl border border-slate-700">
                                <p className="text-xs font-bold uppercase tracking-wider text-slate-400">Low</p>
                                <p className="text-2xl font-black text-sky-400 tabular-nums">{lowCount}</p>
                            </div>
                            <div className="text-center bg-slate-800/50 px-4 py-2 rounded-xl border border-slate-700">
                                <p className="text-xs font-bold uppercase tracking-wider text-slate-400">Dismissed</p>
                                <p className="text-2xl font-black text-emerald-400 tabular-nums">{dismissed.length}</p>
                            </div>
                        </div>
                    </div>

                    {/* Refresh button */}
                    <button
                        onClick={loadAnomalies}
                        disabled={loading}
                        className="absolute top-4 right-4 p-2 text-slate-400 hover:text-white transition-colors disabled:opacity-50 z-20"
                        title="Refresh anomalies"
                    >
                        <span className={`material-symbols-outlined text-xl ${loading ? 'animate-spin' : ''}`}>
                            refresh
                        </span>
                    </button>
                </div>

                {/* ─── Smart Rules Explanation ─── */}
                <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                    <div className="bg-white rounded-xl p-4 border border-slate-100 shadow-sm">
                        <div className="flex items-center gap-2 mb-2">
                            <span className="text-lg">⏱</span>
                            <span className="text-xs font-bold text-slate-600 uppercase tracking-wider">Rule 1: Elapsed Time</span>
                        </div>
                        <p className="text-xs text-slate-500">Measures buckets ÷ time since last collection. Accumulated buckets under trees = normal. Impossible after-pickup spike = alert.</p>
                    </div>
                    <div className="bg-white rounded-xl p-4 border border-slate-100 shadow-sm">
                        <div className="flex items-center gap-2 mb-2">
                            <span className="text-lg">👥</span>
                            <span className="text-xs font-bold text-slate-600 uppercase tracking-wider">Rule 2: Peer Check</span>
                        </div>
                        <p className="text-xs text-slate-500">Compares each picker to their row mates. If everyone is fast = good tree. If ONLY one person is racing = suspicious.</p>
                    </div>
                    <div className="bg-white rounded-xl p-4 border border-slate-100 shadow-sm">
                        <div className="flex items-center gap-2 mb-2">
                            <span className="text-lg">🌅</span>
                            <span className="text-xs font-bold text-slate-600 uppercase tracking-wider">Rule 3: Grace Period</span>
                        </div>
                        <p className="text-xs text-slate-500">First 90 min = warmup. Ladders, cold fruit, no tractors yet. System observes silently, only flags impossible velocity.</p>
                    </div>
                </div>

                {/* ─── Loading State ─── */}
                {loading && (
                    <div className="flex items-center justify-center py-16">
                        <div className="text-center">
                            <div className="w-10 h-10 border-3 border-slate-300 border-t-slate-700 rounded-full animate-spin mx-auto mb-3" />
                            <p className="text-sm text-slate-500 font-medium">Analyzing scan patterns…</p>
                        </div>
                    </div>
                )}

                {/* ─── Filter Chips ─── */}
                {!loading && (
                    <div className="flex gap-2 overflow-x-auto pb-2 items-center">
                        <span className="material-symbols-outlined text-slate-400 text-xl shrink-0 mr-1">filter_list</span>
                        {(Object.keys(FILTER_LABELS) as FilterType[]).map(f => (
                            <button
                                key={f}
                                onClick={() => setFilter(f)}
                                className={`px-4 py-2 rounded-full text-sm font-semibold whitespace-nowrap transition-all ${filter === f
                                    ? 'bg-slate-800 text-white shadow-md scale-105'
                                    : 'bg-white border border-slate-200 text-slate-600 hover:bg-slate-50'
                                    }`}
                            >
                                {FILTER_LABELS[f]}
                                {f !== 'all' && (
                                    <span className="ml-1.5 text-xs opacity-60">
                                        ({anomalies.filter(a => a.type === f).length})
                                    </span>
                                )}
                            </button>
                        ))}
                    </div>
                )}

                {/* ─── Anomaly Cards ─── */}
                {!loading && (
                    <>
                        {filtered.length === 0 ? (
                            <div className="text-center py-16 bg-slate-50 rounded-2xl border border-dashed border-slate-200">
                                <span className="material-symbols-outlined text-5xl text-emerald-400 mb-3 block">verified_user</span>
                                <p className="text-lg font-bold text-slate-700">No anomalies detected</p>
                                <p className="text-sm text-slate-400 mt-1">All scan patterns look normal for this filter</p>
                            </div>
                        ) : (
                            <div className="grid gap-4 md:grid-cols-2">
                                {filtered.map((anomaly, idx) => {
                                    const config = ANOMALY_CONFIG[anomaly.type];
                                    const ruleBadge = RULE_BADGE[anomaly.rule] || RULE_BADGE['elapsed_velocity'];
                                    return (
                                        <div
                                            key={anomaly.id}
                                            onClick={() => openPickerProfile(anomaly.pickerId)}
                                            className={`glass-card card-hover rounded-xl p-5 border border-slate-200 cursor-pointer group flex flex-col justify-between section-enter stagger-${Math.min(idx + 1, 8)}`}
                                        >
                                            <div>
                                                <div className="flex justify-between items-start mb-4">
                                                    <div className="flex gap-3">
                                                        <div className={`p-3 ${config.bg} rounded-xl border border-slate-100`}>
                                                            <span className={`material-symbols-outlined text-xl ${config.color}`}>
                                                                {config.icon}
                                                            </span>
                                                        </div>
                                                        <div>
                                                            <h4 className="font-bold text-slate-800 group-hover:text-primary transition-colors">
                                                                {anomaly.pickerName}
                                                            </h4>
                                                            <p className="text-xs text-slate-400 font-mono mt-0.5 flex items-center gap-1">
                                                                <span className="material-symbols-outlined text-xs">schedule</span>
                                                                {new Date(anomaly.timestamp).toLocaleTimeString([], {
                                                                    hour: '2-digit',
                                                                    minute: '2-digit',
                                                                })}
                                                            </p>
                                                        </div>
                                                    </div>
                                                    <div className="flex flex-col items-end gap-1">
                                                        <span className={`px-2 py-0.5 rounded text-[10px] font-black uppercase tracking-wider border ${SEVERITY_STYLES[anomaly.severity]}`}>
                                                            {anomaly.severity} risk
                                                        </span>
                                                        <span className={`px-2 py-0.5 rounded text-[9px] font-bold ${ruleBadge.color}`}>
                                                            {ruleBadge.label}
                                                        </span>
                                                    </div>
                                                </div>

                                                <div className="bg-slate-50 p-3 rounded-lg border border-slate-100">
                                                    <p className="text-sm text-slate-700 font-medium">{anomaly.detail}</p>
                                                </div>

                                                {/* Evidence pills */}
                                                {anomaly.evidence && (
                                                    <div className="flex flex-wrap gap-1.5 mt-3">
                                                        {Object.entries(anomaly.evidence)
                                                            .filter(([k]) => !['note'].includes(k))
                                                            .slice(0, 4)
                                                            .map(([key, value]) => (
                                                                <span key={key} className="px-2 py-0.5 bg-slate-100 text-slate-500 text-[10px] font-mono rounded">
                                                                    {key.replace(/([A-Z])/g, ' $1').toLowerCase()}: {typeof value === 'object' ? '...' : String(value)}
                                                                </span>
                                                            ))}
                                                    </div>
                                                )}
                                            </div>

                                            <div className="mt-4 pt-3 border-t border-slate-100 flex justify-between items-center text-xs font-bold text-slate-400 group-hover:text-primary transition-colors">
                                                <span>Inspect Profile & History</span>
                                                <span className="material-symbols-outlined text-lg">chevron_right</span>
                                            </div>
                                        </div>
                                    );
                                })}
                            </div>
                        )}
                    </>
                )}

                {/* ─── Smart Dismissals (what the system ignored) ─── */}
                <div className="bg-emerald-50/50 rounded-2xl border border-emerald-200/50 overflow-hidden">
                    <button
                        onClick={() => setShowDismissed(!showDismissed)}
                        className="w-full px-6 py-4 flex justify-between items-center hover:bg-emerald-50 transition-colors"
                    >
                        <div className="flex items-center gap-3">
                            <div className="p-2 bg-emerald-100 rounded-lg">
                                <span className="material-symbols-outlined text-emerald-600">verified_user</span>
                            </div>
                            <div className="text-left">
                                <h3 className="font-bold text-emerald-800 text-sm">Smart Dismissals</h3>
                                <p className="text-xs text-emerald-600">{dismissed.length} scenarios correctly ignored — the system understands your orchard</p>
                            </div>
                        </div>
                        <span className={`material-symbols-outlined text-emerald-500 transition-transform ${showDismissed ? 'rotate-180' : ''}`}>
                            expand_more
                        </span>
                    </button>

                    {showDismissed && (
                        <div className="px-6 pb-5 space-y-3 animate-fade-in">
                            {dismissed.map((ex, i) => (
                                <div key={i} className="bg-white rounded-xl p-4 border border-emerald-100">
                                    <div className="flex items-start gap-3">
                                        <span className="material-symbols-outlined text-emerald-500 text-lg mt-0.5 shrink-0">check_circle</span>
                                        <div>
                                            <p className="text-sm font-bold text-slate-700 mb-1">{ex.scenario}</p>
                                            <p className="text-xs text-slate-500">{ex.reason}</p>
                                            <span className="inline-block mt-2 px-2 py-0.5 bg-emerald-50 text-emerald-600 text-[10px] font-bold rounded">
                                                Rule: {ex.rule}
                                            </span>
                                        </div>
                                    </div>
                                </div>
                            ))}
                        </div>
                    )}
                </div>
            </div>
        </ComponentErrorBoundary>
    );
};

export default AnomalyDetectionView;
