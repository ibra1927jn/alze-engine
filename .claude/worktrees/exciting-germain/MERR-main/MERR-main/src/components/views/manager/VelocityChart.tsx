/**
 * components/views/manager/VelocityChart.tsx
 * Hourly Bucket Velocity Chart (Pure CSS/SVG - No dependencies)
 * Shows last 8 hours of production with elegant zero-data state
 */
import React, { useMemo } from 'react';
import { analyticsService } from '../../../services/analytics.service';
import { BucketRecord } from '../../../types';

interface VelocityChartProps {
    bucketRecords: BucketRecord[];
    targetVelocity?: number; // Target buckets per hour
}

const VelocityChart: React.FC<VelocityChartProps> = ({
    bucketRecords,
    targetVelocity = 50 // Default target for visual reference
}) => {
    const hourlyData = useMemo(() =>
        analyticsService.groupByHour(bucketRecords, 8),
        [bucketRecords]
    );

    const maxCount = useMemo(() => {
        const dataMax = Math.max(...hourlyData.map(d => d.count), 0);
        return Math.max(dataMax, targetVelocity, 10); // Ensure minimum scale
    }, [hourlyData, targetVelocity]);

    const totalToday = hourlyData.reduce((sum, d) => sum + d.count, 0);
    const hasData = totalToday > 0;

    return (
        <div className="bg-white rounded-2xl shadow-sm border border-slate-100 overflow-hidden dash-card-enter anim-delay" style={{ '--delay': '200ms' } as React.CSSProperties}>
            {/* Header */}
            <div className="p-4 border-b border-slate-100 flex justify-between items-center">
                <div>
                    <h3 className="font-bold text-text-main flex items-center gap-2">
                        <span className="material-symbols-outlined text-blue-500">trending_up</span>
                        Velocity (Hourly)
                    </h3>
                    <p className="text-[10px] text-slate-400 mt-0.5">Last 8 hours</p>
                </div>
                <div className="text-right">
                    <p className="text-2xl font-black text-text-main">{totalToday}</p>
                    <p className="text-[10px] text-slate-400">total buckets</p>
                </div>
            </div>

            {/* Chart Area */}
            <div className="p-4">
                {!hasData ? (
                    /* Elegant Zero-Data State */
                    <div className="h-[180px] flex flex-col items-center justify-center text-center">
                        <div className="w-16 h-16 rounded-full bg-slate-100 flex items-center justify-center mb-3">
                            <span className="material-symbols-outlined text-3xl text-slate-300 dash-hourglass">hourglass_empty</span>
                        </div>
                        <p className="text-sm font-bold text-slate-500">Awaiting First Scan</p>
                        <p className="text-xs text-slate-400 mt-1">Data will appear as Runners sync buckets</p>
                        <div className="flex items-center gap-2 mt-3 text-[10px] text-slate-400">
                            <span className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></span>
                            Live updates enabled
                        </div>
                    </div>
                ) : (
                    /* Bar Chart */
                    <div className="h-[180px] flex items-end gap-2">
                        {hourlyData.map((data, idx) => {
                            const height = maxCount > 0 ? (data.count / maxCount) * 100 : 0;
                            const isAboveTarget = data.count >= targetVelocity;
                            const isCurrentHour = idx === hourlyData.length - 1;

                            return (
                                <div key={idx} className="flex-1 flex flex-col items-center gap-1">
                                    {/* Bar */}
                                    <div className="relative w-full h-[140px] flex items-end justify-center">
                                        {/* Target Line (Visual Guide) */}
                                        <div
                                            className="absolute w-full border-t border-dashed border-slate-300 pointer-events-none velocity-target-line"
                                            style={{ '--target-pos': `${(targetVelocity / maxCount) * 100}%` } as React.CSSProperties}
                                        />

                                        {/* Bar */}
                                        <div
                                            className={`w-full max-w-[40px] rounded-t-lg transition-all duration-500 ease-out dynamic-bar ${isCurrentHour
                                                ? 'bg-gradient-to-t from-blue-600 to-blue-400'
                                                : isAboveTarget
                                                    ? 'bg-gradient-to-t from-green-500 to-green-400'
                                                    : 'bg-gradient-to-t from-slate-300 to-slate-200'
                                                }`}
                                            style={{ '--h': `${height}%`, '--min-h': data.count > 0 ? '8px' : '0' } as React.CSSProperties}
                                        >
                                            {/* Count Label */}
                                            {data.count > 0 && (
                                                <div className="absolute -top-5 left-1/2 -translate-x-1/2 text-[10px] font-bold text-text-sub">
                                                    {data.count}
                                                </div>
                                            )}
                                        </div>
                                    </div>

                                    {/* Time Label */}
                                    <span className={`text-[10px] font-medium ${isCurrentHour ? 'text-blue-600 font-bold' : 'text-slate-400'
                                        }`}>
                                        {data.hour.split(':')[0]}
                                    </span>
                                </div>
                            );
                        })}
                    </div>
                )}
            </div>

            {/* Legend */}
            {hasData && (
                <div className="px-4 pb-3 flex items-center justify-center gap-4 text-[10px] text-slate-400">
                    <span className="flex items-center gap-1">
                        <span className="w-3 h-3 rounded bg-blue-500"></span> Current
                    </span>
                    <span className="flex items-center gap-1">
                        <span className="w-3 h-3 rounded bg-green-500"></span> Above Target
                    </span>
                    <span className="flex items-center gap-1">
                        <span className="w-6 border-t border-dashed border-slate-400"></span> Target ({targetVelocity})
                    </span>
                </div>
            )}
        </div>
    );
};

export default VelocityChart;
