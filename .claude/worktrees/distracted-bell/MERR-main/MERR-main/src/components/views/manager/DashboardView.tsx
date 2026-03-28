/**
 * components/views/manager/DashboardView.tsx
 * Executive Dashboard — KPIs with trends, smart projection, performance focus
 */
import React, { useMemo, useCallback, useState, useEffect } from 'react';
import { HarvestState, Picker, BucketRecord, Tab } from '../../../types';
import { useHarvestStore } from '../../../stores/useHarvestStore';
import { analyticsService } from '../../../services/analytics.service';
import { todayNZST } from '@/utils/nzst';
import VelocityChart from './VelocityChart';
import WageShieldPanel from './WageShieldPanel';
import GoalProgress from './GoalProgress';
import PerformanceFocus from './PerformanceFocus';
import TeamLeadersSidebar from './TeamLeadersSidebar';
import { SimulationBanner } from '../../SimulationBanner';
import { TrustBadges } from '../../common/TrustBadges';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';
import { useAnimatedCounter } from '@/hooks/useAnimatedCounter';

interface DashboardViewProps {
    stats: HarvestState['stats'];
    teamLeaders: Picker[];
    crew: Picker[];
    presentCount: number;
    setActiveTab: (tab: Tab) => void;
    bucketRecords?: BucketRecord[];
    onUserSelect?: (user: Partial<Picker>) => void;
}

/* ── Clean Executive Stat Card with animations ─────────────── */

interface StatCardProps {
    title: string;
    value: string | number;
    unit?: string;
    trend?: number;
    icon: string;
    iconBg?: string;
    iconColor?: string;
    onClick?: () => void;
    /** Stagger index for entrance animation (0-3) */
    staggerIndex?: number;
}

const StatCard: React.FC<StatCardProps> = React.memo(({
    title, value, unit, trend, icon,
    iconBg = 'bg-blue-50', iconColor = 'text-blue-600',
    onClick, staggerIndex = 0,
}) => (
    <div
        onClick={onClick}
        className={`bg-white p-4 rounded-2xl border border-slate-100 shadow-sm flex flex-col card-hover section-enter stagger-${Math.min(staggerIndex + 1, 8)} ${onClick ? 'cursor-pointer' : ''}`}
    >
        <div className="flex justify-between items-start mb-2">
            <span className="text-sm text-slate-500 font-medium uppercase tracking-wider">
                {title}
            </span>
            <div className={`${iconBg} p-1.5 rounded-lg ${iconColor} transition-transform duration-300 group-hover:scale-110`}>
                <span className="material-symbols-outlined text-[20px]">{icon}</span>
            </div>
        </div>

        <div className="flex items-baseline gap-1.5">
            <h3 className="text-3xl font-bold text-slate-900 tabular-nums">{value}</h3>
            {unit && <span className="text-sm text-slate-400 font-normal">{unit}</span>}
        </div>

        {trend !== undefined && trend !== 0 ? (
            <div className={`flex items-center gap-1 mt-3 text-sm font-medium self-start px-2 py-0.5 rounded-full animate-slide-up stagger-${Math.min(staggerIndex + 1, 8)} ${trend > 0
                ? 'text-emerald-600 bg-emerald-50'
                : 'text-red-600 bg-red-50'
                }`}
            >
                <span className="material-symbols-outlined text-[16px]">
                    {trend > 0 ? 'trending_up' : 'trending_down'}
                </span>
                <span>{trend > 0 ? '+' : ''}{trend}% vs yesterday</span>
            </div>
        ) : null}
    </div>
));

/* ── Dashboard View ────────────────────────────────────────── */

const DashboardView: React.FC<DashboardViewProps> = ({ stats, teamLeaders, crew = [], presentCount = 0, setActiveTab, bucketRecords = [], onUserSelect }) => {
    const { settings, orchard } = useHarvestStore();

    // 1. Calculate Velocity (Buckets/Hr) - Last 2 Hours
    const velocity = useMemo(() => {
        if (!bucketRecords.length) return 0;
        const now = Date.now();
        const twoHoursAgo = now - (2 * 60 * 60 * 1000);

        const recentCount = bucketRecords.filter((r: BucketRecord) =>
            new Date(r.created_at || r.scanned_at || '').getTime() > twoHoursAgo
        ).length;

        return Math.round(recentCount / 2);
    }, [bucketRecords]);

    // 1b. Production trend vs yesterday
    const productionTrend = useMemo(() => {
        if (!bucketRecords.length) return 0;
        const today = todayNZST();
        const todayDate = new Date(today);
        const yesterdayDate = new Date(todayDate);
        yesterdayDate.setDate(yesterdayDate.getDate() - 1);
        const yesterdayStr = yesterdayDate.toISOString().split('T')[0];

        const todayCount = bucketRecords.filter((r: BucketRecord) => {
            const d = (r.created_at || r.scanned_at || '').substring(0, 10);
            return d === today;
        }).length;

        const yesterdayCount = bucketRecords.filter((r: BucketRecord) => {
            const d = (r.created_at || r.scanned_at || '').substring(0, 10);
            return d === yesterdayStr;
        }).length;

        if (yesterdayCount === 0) return 0;
        return Math.round(((todayCount - yesterdayCount) / yesterdayCount) * 100);
    }, [bucketRecords]);

    // 2. Financial Calculations
    const payroll = useHarvestStore(state => state.payroll);
    const alerts = useHarvestStore(state => state.alerts);
    const totalCost = (bucketRecords.length > 0) ? (payroll?.finalTotal || 0) : 0;

    // Animated counters for stat cards (staggered delays)
    const animVelocity = useAnimatedCounter(velocity, 1000, 200);
    const animBuckets = useAnimatedCounter(stats.totalBuckets, 1400, 300);
    const animCost = useAnimatedCounter(Math.round(totalCost), 1600, 400);
    const animCrew = useAnimatedCounter(presentCount, 800, 500);

    // 3. Progress & ETA
    const target = settings.target_tons || 40;
    const progress = Math.min(100, (stats.tons / target) * 100);

    // 4. ETA Calculation
    const etaInfo = useMemo(() => {
        return analyticsService.calculateETA(stats.tons, target, velocity, 72);
    }, [stats.tons, target, velocity]);

    // 5. Hours elapsed since first bucket
    const hoursElapsed = useMemo(() => {
        if (!bucketRecords.length) return 0;
        const timestamps = bucketRecords.map((r: BucketRecord) =>
            new Date(r.created_at || r.scanned_at || '').getTime()
        );
        const earliest = Math.min(...timestamps);
        return (Date.now() - earliest) / (1000 * 60 * 60);
    }, [bucketRecords]);

    // 6. Export Handler
    const handleExport = useCallback(() => {
        const now = new Date();
        const metadata = {
            generated_at: now.toLocaleString(),
            last_sync: now.toLocaleString(),
            pending_queue_count: 0,
            orchard_name: orchard?.name || 'Orchard',
            is_offline_data: !navigator.onLine
        };

        const csv = analyticsService.generateDailyReport(
            crew,
            bucketRecords,
            { piece_rate: settings.piece_rate || 6.50, min_wage_rate: settings.min_wage_rate || 23.50 },
            teamLeaders,
            metadata
        );

        const filename = `harvest_report_${todayNZST()}.csv`;
        analyticsService.downloadCSV(csv, filename);
    }, [crew, bucketRecords, settings, teamLeaders, orchard?.name]);

    // Live clock (updates every minute)
    const [currentTime, setCurrentTime] = useState(new Date());
    useEffect(() => {
        const timer = setInterval(() => setCurrentTime(new Date()), 60_000);
        return () => clearInterval(timer);
    }, []);

    // Estimated remaining work time (assuming 5pm NZ end)
    const remainingHours = useMemo(() => {
        const nzHour = currentTime.getUTCHours() + 13; // NZDT = UTC+13
        const nzMinute = currentTime.getUTCMinutes();
        const endHour = 17; // 5pm
        const remaining = endHour - nzHour - (nzMinute / 60);
        return Math.max(0, remaining);
    }, [currentTime]);

    const nzTimeStr = currentTime.toLocaleTimeString('en-NZ', {
        hour: '2-digit', minute: '2-digit',
        timeZone: 'Pacific/Auckland',
        hour12: true,
    });

    // Empty state when no data at all
    const isEmpty = crew.length === 0 && bucketRecords.length === 0;

    if (isEmpty) {
        return (
            <div className="p-4 md:p-6 max-w-7xl mx-auto pb-24 animate-fade-in">
                <SimulationBanner />
                <TrustBadges />
                <div className="flex flex-col items-center justify-center py-16 text-center">
                    <div className="w-20 h-20 rounded-full bg-indigo-50 flex items-center justify-center mb-6">
                        <span className="material-symbols-outlined text-4xl text-indigo-400">agriculture</span>
                    </div>
                    <h2 className="text-2xl font-black text-text-main mb-2">No Harvest Data Yet</h2>
                    <p className="text-text-muted max-w-md mb-8">
                        Add your crew and start scanning buckets to see live KPIs, velocity tracking, and cost projections here.
                    </p>
                    <div className="flex gap-3">
                        <button
                            onClick={() => setActiveTab('teams')}
                            className="gradient-primary glow-primary text-white px-5 py-2.5 rounded-xl font-bold text-sm shadow-lg hover:scale-105 transition-all flex items-center gap-2"
                        >
                            <span className="material-symbols-outlined text-lg">group_add</span>
                            Add Pickers
                        </button>
                        <button
                            onClick={() => setActiveTab('map')}
                            className="glass-card text-text-sub px-5 py-2.5 font-bold text-sm hover:bg-slate-50 transition-all flex items-center gap-2"
                        >
                            <span className="material-symbols-outlined text-lg">map</span>
                            View Map
                        </button>
                    </div>
                </div>
            </div>
        );
    }

    return (
        <div className="p-4 md:p-6 space-y-6 max-w-7xl mx-auto pb-24 animate-fade-in">
            <SimulationBanner />
            <TrustBadges />

            {/* Header */}
            <div className="flex flex-col md:flex-row justify-between md:items-end gap-4">
                <div>
                    <h1 className="text-2xl font-black text-text-main">Orchard Overview</h1>
                    <p className="text-sm text-text-muted font-medium">Live monitoring • {orchard?.name || 'Orchard'}</p>
                    <div className="flex items-center gap-3 mt-1.5">
                        <span className="inline-flex items-center gap-1.5 text-xs font-semibold text-slate-600 bg-slate-100 px-2.5 py-1 rounded-full">
                            <span className="material-symbols-outlined text-sm">schedule</span>
                            {nzTimeStr}
                        </span>
                        {remainingHours > 0 && (
                            <span className={`inline-flex items-center gap-1.5 text-xs font-semibold px-2.5 py-1 rounded-full ${remainingHours <= 1 ? 'text-amber-700 bg-amber-50' : 'text-emerald-700 bg-emerald-50'
                                }`}>
                                <span className="material-symbols-outlined text-sm">hourglass_top</span>
                                {remainingHours.toFixed(1)}h remaining
                            </span>
                        )}
                        {remainingHours <= 0 && (
                            <span className="inline-flex items-center gap-1.5 text-xs font-semibold text-red-600 bg-red-50 px-2.5 py-1 rounded-full">
                                <span className="material-symbols-outlined text-sm">timer_off</span>
                                Overtime
                            </span>
                        )}
                    </div>
                </div>
                <div className="flex gap-2 flex-wrap">
                    <button
                        onClick={handleExport}
                        className="bg-white border border-primary/30 text-primary px-4 py-2.5 rounded-xl font-bold text-sm hover:bg-primary/5 hover:border-primary/50 transition-all flex items-center gap-2 shadow-sm"
                    >
                        <span className="material-symbols-outlined text-lg">download</span>
                        Export
                    </button>
                    <button
                        onClick={() => setActiveTab('map')}
                        className="gradient-primary glow-primary text-white px-4 py-2.5 rounded-xl font-bold text-sm shadow-lg hover:scale-105 transition-all flex items-center gap-2"
                    >
                        <span className="material-symbols-outlined text-lg">map</span>
                        Live Map
                    </button>
                </div>
            </div>

            {/* KPI Grid — Animated Executive Cards */}
            <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
                <StatCard
                    title="Velocity"
                    value={animVelocity}
                    unit="b/hr"
                    icon="speed"
                    iconBg="bg-blue-50"
                    iconColor="text-blue-600"
                    onClick={() => setActiveTab('map')}
                    staggerIndex={0}
                />
                <StatCard
                    title="Production"
                    value={animBuckets}
                    unit="buckets"
                    trend={productionTrend}
                    icon="inventory_2"
                    iconBg="bg-indigo-50"
                    iconColor="text-indigo-600"
                    onClick={() => setActiveTab('logistics')}
                    staggerIndex={1}
                />
                <StatCard
                    title="Est. Cost"
                    value={`$${animCost.toLocaleString(undefined, { maximumFractionDigits: 0 })}`}
                    unit="NZD"
                    icon="payments"
                    iconBg="bg-green-50"
                    iconColor="text-green-600"
                    onClick={() => setActiveTab('analytics')}
                    staggerIndex={2}
                />
                <StatCard
                    title="Active Crew"
                    value={animCrew}
                    unit="pickers"
                    icon="groups"
                    iconBg="bg-purple-50"
                    iconColor="text-purple-600"
                    onClick={() => setActiveTab('teams')}
                    staggerIndex={3}
                />
            </div>

            {/* Main Content Split */}
            <div className="grid lg:grid-cols-3 gap-6">
                {/* Left Col */}
                <div className="lg:col-span-2 space-y-6">
                    <GoalProgress
                        progress={progress}
                        currentTons={stats.tons}
                        targetTons={target}
                        eta={etaInfo.eta}
                        etaStatus={etaInfo.status}
                        velocity={velocity}
                        totalBuckets={stats.totalBuckets}
                        hoursElapsed={hoursElapsed}
                    />
                    <ComponentErrorBoundary componentName="Velocity Chart">
                        <VelocityChart
                            bucketRecords={bucketRecords}
                            targetVelocity={Math.round((settings.min_buckets_per_hour || 3.6) * crew.length / 2)}
                        />
                    </ComponentErrorBoundary>
                    {/* Performance Focus: Top 3 + Needs Attention */}
                    <PerformanceFocus
                        crew={crew}
                        bucketRecords={bucketRecords}
                        setActiveTab={setActiveTab}
                        onUserSelect={onUserSelect}
                    />
                </div>

                {/* Right Col */}
                <div className="space-y-4">
                    <WageShieldPanel
                        crew={crew}
                        teamLeaders={teamLeaders}
                        settings={{ piece_rate: settings.piece_rate || 6.50, min_wage_rate: settings.min_wage_rate || 23.50 }}
                        alerts={alerts}
                        onUserSelect={onUserSelect}
                    />
                    <TeamLeadersSidebar
                        teamLeaders={teamLeaders}
                        crew={crew}
                        setActiveTab={setActiveTab}
                        onUserSelect={onUserSelect}
                    />
                </div>
            </div>
        </div>
    );
};

export default DashboardView;
