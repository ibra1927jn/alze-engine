/**
 * PerformanceFocus — Top performers + Needs attention panels
 * 
 * Replaces the plain crew list on the Dashboard with actionable
 * executive insights: who's leading and who needs help.
 */
import React, { useMemo } from 'react';
import { Picker, BucketRecord } from '../../../types';
import { Tab } from '../../../types';

interface PerformanceFocusProps {
    crew: Picker[];
    bucketRecords: BucketRecord[];
    setActiveTab: (tab: Tab) => void;
    onUserSelect?: (user: Partial<Picker>) => void;
}

const PerformanceFocus: React.FC<PerformanceFocusProps> = ({
    crew,
    bucketRecords,
    setActiveTab,
    onUserSelect,
}) => {
    // Calculate per-picker bucket counts
    const rankedPickers = useMemo(() => {
        const counts: Record<string, { id: string; name: string; count: number }> = {};

        bucketRecords.forEach((r: BucketRecord) => {
            const id = r.picker_id || 'unknown';
            if (!counts[id]) {
                counts[id] = { id, name: r.picker_name || 'Unknown', count: 0 };
            }
            counts[id].count++;
        });

        // Merge with crew to catch pickers with 0 buckets
        crew.forEach(p => {
            if (p.role === 'picker' && !counts[p.id]) {
                counts[p.id] = { id: p.id, name: p.name, count: 0 };
            }
        });

        return Object.values(counts).sort((a, b) => b.count - a.count);
    }, [crew, bucketRecords]);

    const avgBuckets = rankedPickers.length > 0
        ? Math.round(rankedPickers.reduce((s, p) => s + p.count, 0) / rankedPickers.length)
        : 0;

    const topPerformers = rankedPickers.slice(0, 3);
    const needsAttention = rankedPickers
        .filter(p => p.count < avgBuckets && p.count >= 0)
        .slice(-3)
        .reverse();

    const handlePickerClick = (picker: { id: string; name: string }) => {
        if (onUserSelect) {
            onUserSelect({ id: picker.id, name: picker.name, role: 'picker' });
        }
    };

    if (rankedPickers.length === 0) return null;

    return (
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {/* Top Performers */}
            <div className="bg-emerald-50/50 p-4 rounded-2xl border border-emerald-100 dash-card-enter anim-delay" style={{ '--delay': '300ms' } as React.CSSProperties}>
                <h4 className="text-sm font-bold text-emerald-800 mb-3 flex items-center gap-2">
                    <span className="material-symbols-outlined text-base">trophy</span>
                    Top {topPerformers.length} Today
                </h4>
                {topPerformers.length > 0 ? (
                    <ul className="space-y-2">
                        {topPerformers.map((p, i) => (
                            <li
                                key={p.id}
                                onClick={() => handlePickerClick(p)}
                                className="flex justify-between items-center text-sm cursor-pointer hover:bg-emerald-100/50 rounded-lg px-2 py-1.5 -mx-2 transition-colors"
                            >
                                <span className="flex items-center gap-2">
                                    <span className="w-5 h-5 rounded-full bg-emerald-200 text-emerald-800 text-[10px] font-bold flex items-center justify-center">
                                        {i + 1}
                                    </span>
                                    <span className="font-medium text-slate-700">{p.name}</span>
                                </span>
                                <span className="font-bold text-emerald-700">{p.count} 🪣</span>
                            </li>
                        ))}
                    </ul>
                ) : (
                    <p className="text-sm text-emerald-600/60">No data yet</p>
                )}
            </div>

            {/* Needs Attention */}
            <div className="bg-amber-50/50 p-4 rounded-2xl border border-amber-100 dash-card-enter anim-delay" style={{ '--delay': '400ms' } as React.CSSProperties}>
                <h4 className="text-sm font-bold text-amber-800 mb-3 flex items-center gap-2">
                    <span className="material-symbols-outlined text-base">help</span>
                    Below Average (&lt; {avgBuckets})
                </h4>
                {needsAttention.length > 0 ? (
                    <>
                        <ul className="space-y-2">
                            {needsAttention.map(p => (
                                <li
                                    key={p.id}
                                    onClick={() => handlePickerClick(p)}
                                    className="flex justify-between items-center text-sm cursor-pointer hover:bg-amber-100/50 rounded-lg px-2 py-1.5 -mx-2 transition-colors"
                                >
                                    <span className="font-medium text-slate-600">{p.name}</span>
                                    <span className="font-bold text-amber-700">{p.count} 🪣</span>
                                </li>
                            ))}
                        </ul>
                        <button
                            onClick={() => setActiveTab('teams')}
                            className="text-xs text-amber-700 font-bold mt-3 w-full text-right hover:text-amber-900 transition-colors"
                        >
                            View all in Teams →
                        </button>
                    </>
                ) : (
                    <p className="text-sm text-amber-600/60">Everyone is on track 🎉</p>
                )}
            </div>
        </div>
    );
};

export default React.memo(PerformanceFocus);
