import React, { useMemo } from 'react';
import { useHarvestStore as useHarvest } from '@/stores/useHarvestStore';

const HomeView = ({ onNavigate }: { onNavigate?: (tab: string) => void }) => {
    const { currentUser, stats, crew, settings } = useHarvest();

    // 1. Crew Stats (Real)
    const totalCrew = crew.length;

    // 2. Safety Status (Real Monitor)
    // Scan for ANY picker with 'suspended' or 'issue' status.
    // Real-time: Listens to 'pickers' changes in Context
    const safetyIssuePicker = crew.find(p => p.status === 'suspended' || p.status === 'issue');
    const safetyStatus = safetyIssuePicker ? 'issue' : 'safe';

    // 3. Performance Analytics (Real Goal)
    // Goal is relative to Manager's setting (min_buckets_per_hour)
    // Source of Truth: harvest_settings table (via Context)
    const dailyGoalPerPicker = (settings?.min_buckets_per_hour || 3.6) * 8; // Approx 8 hour day target
    const currentAvg = totalCrew > 0 ? (stats.totalBuckets / totalCrew) : 0;
    const progressPercent = Math.min((currentAvg / dailyGoalPerPicker) * 100, 100);

    const cards = [
        { label: 'Buckets', value: stats.totalBuckets, icon: 'shopping_basket', color: 'primary' },
        { label: 'Pay Est.', value: `$${stats.payEstimate.toFixed(0)}`, icon: 'payments', color: 'text-main' },
        { label: 'Tons', value: stats.tons.toFixed(1), icon: 'scale', color: 'text-main' },
    ];

    // Ordenar crew por rendimiento (buckets)
    const rankedCrew = useMemo(() => {
        return [...crew].sort((a, b) => (b.total_buckets_today || 0) - (a.total_buckets_today || 0));
    }, [crew]);

    return (
        <div className="bg-background-light min-h-screen pb-24">
            {/* Header Section */}
            <header className="bg-surface-white px-6 pt-12 pb-6 border-b border-border-light sticky top-0 z-20 shadow-sm">
                <div className="flex justify-between items-start mb-6">
                    <div onClick={() => onNavigate && onNavigate('profile')} className="cursor-pointer">
                        <h1 className="text-3xl font-black text-text-main tracking-tight">
                            Kia Ora, {currentUser?.name?.split(' ')[0] || 'Team Leader'}
                        </h1>
                        <p className="text-text-sub font-medium mt-1">
                            {new Date().toLocaleDateString('en-NZ', { weekday: 'long', day: 'numeric', month: 'long' })}
                        </p>
                    </div>
                    <div
                        onClick={() => onNavigate && onNavigate('profile')}
                        className="size-10 rounded-full bg-primary/10 flex items-center justify-center border border-primary/20 cursor-pointer"
                    >
                        {/* Avatar Real */}
                        <span className="text-primary font-bold text-sm">
                            {currentUser?.name ? currentUser.name.substring(0, 2).toUpperCase() : 'TL'}
                        </span>
                    </div>
                </div>

                {/* KPI Grid */}
                <div className="grid grid-cols-3 gap-3">
                    {cards.map((card, i) => (
                        <KpiCard key={i} {...card} />
                    ))}
                </div>
            </header>

            <main className="px-4 mt-6 space-y-6">
                {/* 1. SAFETY MONITOR (REAL DATA) */}
                <div
                    onClick={() => onNavigate && onNavigate('team')}
                    className={`rounded-2xl p-5 border-l-4 shadow-sm flex items-center justify-between transition-colors duration-300 cursor-pointer active:scale-[0.98] ${safetyStatus === 'issue'
                        ? 'bg-red-50 border-l-red-500' // Alerta Roja
                        : 'bg-surface-white border-l-emerald-500' // Verde Seguro
                        }`}>
                    <div>
                        <h3 className={`font-bold text-lg ${safetyStatus === 'issue' ? 'text-red-700' : 'text-text-main'}`}>
                            {safetyStatus === 'issue' ? 'Action Required' : 'Morning Huddle'}
                        </h3>
                        <p className={`text-xs font-semibold mt-1 ${safetyStatus === 'issue' ? 'text-red-500' : 'text-text-sub'}`}>
                            {safetyStatus === 'issue'
                                ? `Check: ${safetyIssuePicker?.name} (${safetyIssuePicker?.status?.toUpperCase()})`
                                : 'All crew active & verified.'
                            }
                        </p>
                    </div>
                    <div className={`size-10 rounded-full flex items-center justify-center ${safetyStatus === 'issue' ? 'bg-red-100 text-red-600' : 'bg-green-100 text-emerald-500'
                        }`}>
                        <span className="material-symbols-outlined">
                            {safetyStatus === 'issue' ? 'warning' : 'check_circle'}
                        </span>
                    </div>
                </div>

                {/* 2. PERFORMANCE ANALYTICS (REAL GOAL) */}
                <div onClick={() => onNavigate && onNavigate('tasks')} className="cursor-pointer">
                    <div className="flex justify-between items-end mb-3">
                        <h2 className="text-text-main font-bold text-lg">Crew Performance</h2>
                        <div className="text-right">
                            <span className="text-[10px] font-bold text-text-sub uppercase">Team Target</span>
                            <p className="text-xs font-bold text-primary-vibrant">
                                {settings?.min_buckets_per_hour || 3.6} bkt/hr
                            </p>
                        </div>
                    </div>
                    <div className="bg-surface-white p-5 rounded-2xl border border-border-light shadow-sm">
                        <div className="flex justify-between text-sm font-bold text-text-sub mb-2">
                            <span>Daily Progress</span>
                            <span>{Math.round(progressPercent)}%</span>
                        </div>
                        <div className="w-full bg-background-light rounded-full h-3 overflow-hidden relative">
                            {/* Progress Bar */}
                            <div
                                className="bg-primary-vibrant h-full rounded-full transition-all duration-1000 ease-out shadow-[0_0_10px_rgba(255,31,61,0.4)] dynamic-width"
                                style={{ '--w': `${progressPercent}%` } as React.CSSProperties}
                            ></div>
                        </div>
                        <p className="text-center text-[10px] text-text-sub font-medium mt-3">
                            {/* Feedback del estado */}
                            {progressPercent < 50 ? 'Pace is slow. Encouragement needed.' : 'Good pace! Keep it up.'}
                        </p>
                    </div>
                </div>

                {/* MY CREW LIST */}
                <section>
                    <div className="flex justify-between items-center mb-4">
                        <h3 className="font-bold text-text-main text-lg flex items-center gap-2">
                            <span className="material-symbols-outlined text-primary">group</span>
                            Active Crew ({totalCrew})
                        </h3>
                        <button
                            onClick={() => onNavigate && onNavigate('team')}
                            className="text-primary-vibrant text-xs font-bold uppercase"
                        >
                            View All
                        </button>
                    </div>

                    <div className="space-y-3">
                        {rankedCrew.slice(0, 5).map(picker => (
                            <div key={picker.id} className="bg-surface-white p-4 rounded-2xl border border-border-light shadow-sm flex items-center justify-between">
                                <div className="flex items-center gap-4">
                                    <div className="size-12 rounded-full bg-background-light flex items-center justify-center text-text-sub font-bold border border-border-light text-sm">
                                        {picker.avatar || (picker.name || '??').substring(0, 2).toUpperCase()}
                                    </div>
                                    <div className="flex flex-col">
                                        <h4 className="font-bold text-text-main leading-tight">{picker.name}</h4>
                                        <div className="flex items-center gap-2 mt-1">
                                            {/* Row/Bench Status */}
                                            {!picker.orchard_id ? (
                                                <span className="text-[10px] font-bold uppercase text-text-muted bg-slate-100 px-1.5 py-0.5 rounded border border-border-light">
                                                    Bench
                                                </span>
                                            ) : (
                                                <span className="text-[10px] font-bold uppercase text-primary bg-primary/10 px-1.5 py-0.5 rounded border border-primary/20">
                                                    Row {picker.current_row || '-'}
                                                </span>
                                            )}

                                            {/* QC Dots */}
                                            <div className="flex gap-0.5 opacity-80">
                                                {(picker.qcStatus || [1, 1, 1]).map((status, i) => (
                                                    <div key={i} className={`size-1.5 rounded-full ${status === 1 ? 'bg-success' : 'bg-warning'}`} />
                                                ))}
                                            </div>
                                        </div>
                                    </div>
                                </div>
                                <div className="text-right">
                                    <span className="block text-2xl font-black text-text-main leading-none">
                                        {picker.total_buckets_today || 0}
                                    </span>
                                    <span className="text-[10px] font-bold text-text-sub uppercase">Buckets</span>
                                </div>
                            </div>
                        ))}
                    </div>
                </section>
            </main>
        </div>
    );
};

interface KpiCardProps {
    label: string;
    value: string | number;
    icon: string;
    color: string;
}

const KpiCard = ({ label, value, icon, color }: KpiCardProps) => (
    <div className="bg-background-light p-3 rounded-2xl border border-border-light flex flex-col items-center justify-center h-24">
        <span className={`material-symbols-outlined text-2xl mb-1 ${color === 'primary' ? 'text-primary' : 'text-text-sub'}`}>
            {icon}
        </span>
        <span className={`text-xl font-black ${color === 'primary' ? 'text-primary' : 'text-text-main'}`}>
            {value}
        </span>
        <span className="text-[10px] font-bold text-text-sub uppercase tracking-wider">{label}</span>
    </div>
);

export default HomeView;
