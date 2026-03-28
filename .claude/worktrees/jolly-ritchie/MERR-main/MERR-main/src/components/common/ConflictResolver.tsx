/**
 * ConflictResolver.tsx — Scheduling Conflict Resolution UI
 *
 * Shows detected conflicts between pickers, schedules, or assignments
 * and lets managers resolve them with a single tap.
 *
 * Designed for high-density orchards where double-bookings and overlap
 * create throughput bottlenecks.
 */
import React, { useState } from 'react';

/* ── Types ── */
export interface Conflict {
    id: string;
    type: 'double_booking' | 'zone_overlap' | 'visa_expired' | 'contract_gap';
    severity: 'critical' | 'warning' | 'info';
    title: string;
    description: string;
    /** Names/IDs of affected entities */
    affectedEntities: string[];
    /** Suggested resolution actions */
    resolutions: Resolution[];
    detectedAt: string;
}

export interface Resolution {
    id: string;
    label: string;
    icon: string;
    /** If true, highlighted as recommended */
    recommended?: boolean;
}

interface ConflictResolverProps {
    conflicts: Conflict[];
    onResolve: (conflictId: string, resolutionId: string) => void;
    onDismiss: (conflictId: string) => void;
}

const SEVERITY_STYLES: Record<string, { bg: string; border: string; icon: string; iconColor: string; badge: string }> = {
    critical: {
        bg: 'bg-red-50',
        border: 'border-red-200',
        icon: 'error',
        iconColor: 'text-red-500',
        badge: 'bg-red-100 text-red-700',
    },
    warning: {
        bg: 'bg-amber-50',
        border: 'border-amber-200',
        icon: 'warning',
        iconColor: 'text-amber-500',
        badge: 'bg-amber-100 text-amber-700',
    },
    info: {
        bg: 'bg-blue-50',
        border: 'border-blue-200',
        icon: 'info',
        iconColor: 'text-blue-500',
        badge: 'bg-blue-100 text-blue-700',
    },
};

const TYPE_LABELS: Record<string, string> = {
    double_booking: 'Double Booking',
    zone_overlap: 'Zone Overlap',
    visa_expired: 'Visa Expired',
    contract_gap: 'Contract Gap',
};

const ConflictResolver: React.FC<ConflictResolverProps> = ({ conflicts, onResolve, onDismiss }) => {
    const [expandedId, setExpandedId] = useState<string | null>(null);
    const [resolvedIds, setResolvedIds] = useState<Set<string>>(new Set());

    const activeConflicts = conflicts.filter(c => !resolvedIds.has(c.id));
    const criticalCount = activeConflicts.filter(c => c.severity === 'critical').length;
    const warningCount = activeConflicts.filter(c => c.severity === 'warning').length;

    const handleResolve = (conflictId: string, resolutionId: string) => {
        onResolve(conflictId, resolutionId);
        setResolvedIds(prev => new Set(prev).add(conflictId));
        setExpandedId(null);
    };

    const handleDismiss = (conflictId: string) => {
        onDismiss(conflictId);
        setResolvedIds(prev => new Set(prev).add(conflictId));
    };

    if (activeConflicts.length === 0) {
        return (
            <div className="bg-emerald-50 border border-emerald-200 rounded-xl p-5 text-center">
                <span className="material-symbols-outlined text-emerald-500 text-3xl mb-2 block">check_circle</span>
                <p className="text-sm font-bold text-emerald-700">No Active Conflicts</p>
                <p className="text-xs text-emerald-600 mt-1">All scheduling conflicts have been resolved</p>
            </div>
        );
    }

    return (
        <div className="space-y-3">
            {/* Summary Bar */}
            <div className="flex items-center gap-3 bg-white rounded-xl p-3 shadow-sm border border-border-light">
                <span className="material-symbols-outlined text-indigo-600 text-xl">gavel</span>
                <div className="flex-1">
                    <p className="text-sm font-bold text-text-primary">
                        {activeConflicts.length} Active Conflict{activeConflicts.length !== 1 ? 's' : ''}
                    </p>
                </div>
                <div className="flex gap-1.5">
                    {criticalCount > 0 && (
                        <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full bg-red-100 text-red-700 text-[10px] font-bold">
                            <span className="w-1.5 h-1.5 rounded-full bg-red-500" />
                            {criticalCount} critical
                        </span>
                    )}
                    {warningCount > 0 && (
                        <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full bg-amber-100 text-amber-700 text-[10px] font-bold">
                            <span className="w-1.5 h-1.5 rounded-full bg-amber-500" />
                            {warningCount} warning
                        </span>
                    )}
                </div>
            </div>

            {/* Conflict Cards */}
            {activeConflicts
                .sort((a, b) => {
                    const sev = { critical: 0, warning: 1, info: 2 };
                    return (sev[a.severity] ?? 2) - (sev[b.severity] ?? 2);
                })
                .map(conflict => {
                    const style = SEVERITY_STYLES[conflict.severity] || SEVERITY_STYLES.info;
                    const isExpanded = expandedId === conflict.id;
                    return (
                        <div key={conflict.id} className={`rounded-xl border ${style.border} ${style.bg} overflow-hidden transition-all`}>
                            {/* Conflict Header */}
                            <button
                                className="w-full flex items-center gap-3 px-4 py-3 text-left"
                                onClick={() => setExpandedId(isExpanded ? null : conflict.id)}
                            >
                                <span className={`material-symbols-outlined ${style.iconColor}`}>{style.icon}</span>
                                <div className="flex-1 min-w-0">
                                    <div className="flex items-center gap-2 mb-0.5">
                                        <p className="text-sm font-bold text-text-primary truncate">{conflict.title}</p>
                                        <span className={`text-[9px] font-bold uppercase px-1.5 py-0.5 rounded-full ${style.badge}`}>
                                            {TYPE_LABELS[conflict.type] || conflict.type}
                                        </span>
                                    </div>
                                    <p className="text-xs text-text-secondary truncate">{conflict.description}</p>
                                </div>
                                <span className={`material-symbols-outlined text-text-muted transition-transform ${isExpanded ? 'rotate-180' : ''}`}>
                                    expand_more
                                </span>
                            </button>

                            {/* Expanded Details */}
                            {isExpanded && (
                                <div className="px-4 pb-4 space-y-3">
                                    {/* Affected Entities */}
                                    <div className="flex flex-wrap gap-1.5">
                                        {conflict.affectedEntities.map(entity => (
                                            <span key={entity} className="inline-flex items-center gap-1 px-2 py-1 rounded-lg bg-white/80 text-xs font-medium text-text-primary border border-border-light/60">
                                                <span className="material-symbols-outlined text-xs text-text-muted">person</span>
                                                {entity}
                                            </span>
                                        ))}
                                    </div>

                                    {/* Resolution Options */}
                                    <div className="space-y-1.5">
                                        <p className="text-[10px] font-bold uppercase text-text-secondary tracking-wide">Resolution Options</p>
                                        {conflict.resolutions.map(res => (
                                            <button
                                                key={res.id}
                                                onClick={() => handleResolve(conflict.id, res.id)}
                                                className={`w-full flex items-center gap-2 px-3 py-2.5 rounded-lg text-sm font-medium transition-all ${res.recommended
                                                        ? 'bg-indigo-600 text-white hover:bg-indigo-700 shadow-sm'
                                                        : 'bg-white text-text-primary border border-border-light hover:border-border-medium hover:bg-background-light'
                                                    }`}
                                            >
                                                <span className="material-symbols-outlined text-sm">{res.icon}</span>
                                                {res.label}
                                                {res.recommended && (
                                                    <span className="ml-auto text-[9px] uppercase font-bold bg-white/20 px-1.5 py-0.5 rounded-full">
                                                        Recommended
                                                    </span>
                                                )}
                                            </button>
                                        ))}
                                    </div>

                                    {/* Dismiss */}
                                    <button
                                        onClick={() => handleDismiss(conflict.id)}
                                        className="text-xs text-text-muted hover:text-text-secondary font-medium transition-colors"
                                    >
                                        Dismiss this conflict
                                    </button>
                                </div>
                            )}
                        </div>
                    );
                })}
        </div>
    );
};

export default ConflictResolver;
