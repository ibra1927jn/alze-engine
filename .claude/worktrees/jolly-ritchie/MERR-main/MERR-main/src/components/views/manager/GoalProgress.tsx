/**
 * GoalProgress — Daily target with ETA + Smart Projection
 * 
 * Shows current progress AND forward-looking projection:
 * "At current pace, we'll hit X buckets by end of day"
 */
import React, { useState, useEffect } from 'react';

interface GoalProgressProps {
    progress: number;
    currentTons: number;
    targetTons: number;
    eta: string;
    etaStatus: 'ahead' | 'on_track' | 'behind' | 'complete' | 'no_data';
    velocity?: number;
    totalBuckets?: number;
    hoursElapsed?: number;
}

const GoalProgress: React.FC<GoalProgressProps> = ({
    progress,
    currentTons,
    targetTons,
    eta,
    etaStatus,
    velocity = 0,
    totalBuckets = 0,
    hoursElapsed = 0,
}) => {
    // Animated progress bar — fills from 0 on mount
    const [barWidth, setBarWidth] = useState(0);

    useEffect(() => {
        const timeout = setTimeout(() => setBarWidth(progress), 300);
        return () => clearTimeout(timeout);
    }, [progress]);

    // Smart projection: at current velocity, how many buckets by 8h workday?
    const projectedBuckets = hoursElapsed > 0
        ? Math.round((totalBuckets / hoursElapsed) * 8)
        : 0;
    const targetBuckets = Math.round(targetTons * 72); // ~72 buckets per ton
    const isOnTrack = projectedBuckets >= targetBuckets || etaStatus === 'ahead' || etaStatus === 'complete';

    // Suppress unused var warnings
    void velocity;

    return (
        <div className="bg-gradient-to-br from-indigo-600 to-indigo-700 text-white p-6 !rounded-3xl shadow-xl relative overflow-hidden dash-card-enter">
            <div className="absolute top-0 right-0 w-64 h-64 bg-indigo-400 rounded-full blur-3xl opacity-20 -mr-16 -mt-16 pointer-events-none"></div>
            <div className="relative z-10 flex justify-between items-end mb-4">
                <div>
                    <p className="text-indigo-200 text-xs font-bold uppercase mb-1">Daily Target</p>
                    <h3 className="text-3xl font-black">
                        {progress.toFixed(0)}% <span className="text-lg text-indigo-200 font-medium">Complete</span>
                    </h3>
                </div>
                <div className="text-right">
                    <p className="text-xl font-bold">{currentTons.toFixed(1)} / {targetTons} t</p>
                    <div className={`flex items-center gap-1 mt-1 text-sm font-bold ${etaStatus === 'ahead' ? 'text-green-400' :
                        etaStatus === 'on_track' ? 'text-yellow-400' : 'text-red-400'
                        }`}>
                        <span className="material-symbols-outlined text-sm">
                            {etaStatus === 'ahead' ? 'rocket_launch' :
                                etaStatus === 'on_track' ? 'schedule' : 'warning'}
                        </span>
                        <span>ETA: {eta}</span>
                    </div>
                </div>
            </div>
            {/* Animated Progress Bar */}
            <div className="h-4 bg-white/30 rounded-full overflow-hidden">
                <div
                    className="h-full bg-white rounded-full relative transition-all duration-1000 ease-out dynamic-width"
                    style={{ '--w': `${barWidth}%` } as React.CSSProperties}
                >
                    {/* Shimmer effect on the bar */}
                    {barWidth > 0 && (
                        <div className="absolute inset-0 overflow-hidden rounded-full">
                            <div className="absolute inset-0 bg-gradient-to-r from-transparent via-white/30 to-transparent dash-shimmer" />
                        </div>
                    )}
                </div>
            </div>

            {/* Smart Projection */}
            {hoursElapsed > 0.5 && (
                <div className="relative z-10 mt-4 pt-3 border-t border-white/20 flex justify-between items-center text-sm animate-fade-in anim-delay" style={{ '--delay': '0.5s', animationFillMode: 'both' } as React.CSSProperties}>
                    <span className="text-indigo-200">Projected end-of-day (8h):</span>
                    <span className={`font-bold flex items-center gap-1.5 ${isOnTrack ? 'text-green-300' : 'text-amber-300'}`}>
                        {isOnTrack ? '🟢' : '⚠️'} {projectedBuckets.toLocaleString()} buckets
                    </span>
                </div>
            )}
        </div>
    );
};

export default GoalProgress;
