// components/views/runner/LogisticsView.tsx
import React from 'react';
import { useHarvestStore } from '@/stores/useHarvestStore';
import { useToast } from '@/hooks/useToast';
import Toast from '@/components/ui/Toast';


interface LogisticsViewProps {
    onScan: (type?: 'BIN' | 'BUCKET') => void;
    pendingUploads?: number;
    inventory?: {
        empty_bins?: number;
        raw?: { status: string; sunExposureStart?: number }[];
        [key: string]: unknown;
    };
    onBroadcast?: (message: string) => void;
    selectedBinId?: string;
    // Added from Stashed usage
    _onLogoTap?: () => void;  // Prefix unused params with _
    _onShowHelp?: () => void;
    _sunlightMode?: boolean;
    _onToggleSunlight?: () => void;
}

const LogisticsView: React.FC<LogisticsViewProps> = ({
    onScan,
    _onLogoTap,
    _onShowHelp,
    pendingUploads = 0,
    inventory,
    onBroadcast,
    selectedBinId,
    _sunlightMode,
    _onToggleSunlight
}) => {
    const buckets = useHarvestStore((state) => state.buckets);
    const activeBinBuckets = buckets.filter(r => r.orchard_id === 'offline_pending').length;
    const { toast, showToast, hideToast } = useToast();

    // Removed unused "pop" animation state (was never rendered)

    // 🔍 DEBUG: Log when bucketRecords changes
    React.useEffect(() => {
    }, [buckets]);

    const binCapacity = 72;
    const activeBinPercentage = Math.round((activeBinBuckets / binCapacity) * 100);

    const emptyBins = inventory?.empty_bins || 0;

    // Calculate Max Sun Exposure from raw bins (if passed)
    const [maxExposure, setMaxExposure] = React.useState("00:00:00");

    React.useEffect(() => {
        const timer = setInterval(() => {
            const rawBins = inventory?.raw || [];
            const fullRawBins = rawBins.filter((b: { status: string; sunExposureStart?: number }) => b.status === 'full' && b.sunExposureStart);

            if (fullRawBins.length === 0) {
                setMaxExposure("00:00:00");
                return;
            }

            const now = Date.now();
            const maxDiff = Math.max(...fullRawBins.map(b => now - b.sunExposureStart!));

            const hours = Math.floor(maxDiff / 3600000);
            const minutes = Math.floor((maxDiff % 3600000) / 60000);
            const seconds = Math.floor((maxDiff % 60000) / 1000);

            setMaxExposure(
                `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`
            );
        }, 1000);

        return () => clearInterval(timer);
    }, [inventory]);

    const handleRefillRequest = () => {
        if (onBroadcast) {
            onBroadcast("Runner needs empty bins at Current Zone");
            showToast('Request broadcasted!', 'success');
        }
    };

    return (
        <div className="flex flex-col h-full">
            {/* Header */}
            <header className="flex-none bg-white shadow-sm z-30">
                <div className="flex items-center px-4 py-3 justify-between">
                    <h1 className="text-text-main text-xl font-extrabold tracking-tight">Logistics Hub</h1>
                    <div className="flex items-center gap-3">
                        <button
                            onClick={() => onBroadcast?.("Notification center requested")}
                            className="relative flex items-center justify-center rounded-full size-10 bg-slate-50 text-text-sub"
                        >
                            <span className="material-symbols-outlined">notifications</span>
                            <span className="absolute top-2 right-2.5 size-2 bg-primary rounded-full border-2 border-white"></span>
                        </button>
                        <div className="size-10 rounded-full bg-slate-200 overflow-hidden border border-border-light">
                            <img src={`https://ui-avatars.com/api/?name=Runner&background=random`} alt="Avatar" className="w-full h-full object-cover" />
                        </div>
                    </div>
                </div>
                {/* Offline Banner */}
                {pendingUploads > 0 && (
                    <div className="bg-orange-50 border-y border-orange-100 px-4 py-2.5 flex items-center justify-between">
                        <div className="flex items-center gap-2">
                            <span className="material-symbols-outlined text-orange-600">cloud_off</span>
                            <p className="text-orange-800 text-sm font-bold">Offline Sync Pending</p>
                        </div>
                        <div
                            data-testid="sync-badge"
                            className="flex items-center gap-1.5 bg-orange-200/50 px-2 py-0.5 rounded-full"
                        >
                            <span className="material-symbols-outlined text-orange-700 text-sm">sync</span>
                            <span className="text-xs font-black text-orange-800 uppercase">{pendingUploads} Pending</span>
                        </div>
                    </div>
                )}
            </header>

            {/* Main Content */}
            <div className="flex-1 overflow-y-auto px-4 pt-4 pb-28 space-y-4">
                {/* Active Bin Card */}
                <div className="bg-white rounded-2xl p-5 shadow-sm border border-border-light">
                    <div className="flex items-start justify-between mb-2">
                        <div>
                            <h2 className="text-2xl font-black text-text-main leading-none">
                                {selectedBinId ? `Bin ${selectedBinId}` : 'No Bin Selected'}
                            </h2>
                            <p className="text-sm font-medium text-text-muted mt-1">
                                {selectedBinId ? 'Active Fill Progress' : 'Scan a bin to start'}
                            </p>
                        </div>
                        <span className="px-2 py-1 rounded bg-green-50 text-green-700 text-[10px] font-black uppercase tracking-widest border border-green-100">Active</span>
                    </div>
                    {/* SVG Chart */}
                    <div className="flex items-center justify-center py-4 relative">
                        <div className="w-48 h-48 relative">
                            <svg className="block mx-auto max-w-full h-auto" viewBox="0 0 36 36">
                                <path className="fill-none stroke-[#F1F1F1] stroke-[3]" d="M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831"></path>
                                <path className="fill-none stroke-primary stroke-[3] stroke-linecap-round" strokeDasharray={`${activeBinPercentage}, 100`} d="M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831"></path>
                            </svg>
                            <div className="absolute inset-0 flex flex-col items-center justify-center">
                                <span className="text-4xl font-black text-text-main">{activeBinPercentage}%</span>
                                <span className="text-[10px] font-bold text-text-muted uppercase tracking-widest">Filled</span>
                            </div>
                        </div>
                    </div>
                    <div className="text-center">
                        <p className="text-text-main text-xl font-black">{activeBinBuckets}<span className="text-text-muted font-bold mx-1">/</span>{binCapacity}</p>
                        <p className="text-xs font-bold text-text-muted uppercase tracking-wider mt-0.5">Buckets in current bin</p>
                    </div>
                </div>

                {/* Sun Exposure */}
                <div className="bg-white rounded-2xl p-4 shadow-sm border border-border-light flex items-center justify-between">
                    <div className="flex items-center gap-3">
                        <div className="size-12 rounded-xl bg-orange-100 flex items-center justify-center text-orange-600">
                            <span className="material-symbols-outlined material-icon-filled">wb_sunny</span>
                        </div>
                        <div>
                            <p className="text-xs font-bold text-text-muted uppercase tracking-tight">Sun Exposure</p>
                            <div className="flex items-center gap-1.5 mt-0.5">
                                <span className="size-2 rounded-full bg-emerald-500"></span>
                                <p className="text-sm font-black text-success uppercase tracking-wide">Safe Level</p>
                            </div>
                        </div>
                    </div>
                    <div className="text-right">
                        <p className="text-2xl font-mono font-black text-text-main tabular-nums">{maxExposure}</p>
                    </div>
                </div>

                {/* Supply Management */}
                <div className="bg-white rounded-2xl p-5 shadow-sm border border-border-light">
                    <h3 className="text-sm font-black text-text-main uppercase tracking-widest mb-4">Supply Management</h3>
                    <div className="grid grid-cols-2 gap-4 mb-5">
                        <div className="bg-slate-50 rounded-xl p-3 border border-border-light">
                            <p className="text-[11px] font-bold text-text-muted uppercase">Empty Bins</p>
                            <div className="flex items-baseline justify-between mt-1">
                                <span className="text-2xl font-black text-text-main">{emptyBins}</span>
                                <span className={`text-[10px] font-black uppercase ${emptyBins < 5 ? 'text-primary' : 'text-success'}`}>
                                    {emptyBins < 5 ? 'Low Stock' : 'Good'}
                                </span>
                            </div>
                        </div>
                        <div className="bg-slate-50 rounded-xl p-3 border border-border-light">
                            <p className="text-[11px] font-bold text-text-muted uppercase">Empty Buckets</p>
                            <div className="flex items-baseline justify-between mt-1">
                                <span className="text-2xl font-black text-text-main">{Math.max(10, emptyBins * 5)}</span>
                                <span className="text-[10px] font-black text-success uppercase">Stock OK</span>
                            </div>
                        </div>
                    </div>
                    <button
                        onClick={handleRefillRequest}
                        className="w-full gradient-primary glow-primary text-white py-4 rounded-xl font-black text-sm uppercase tracking-widest flex items-center justify-center gap-2 active:scale-95 transition-transform"
                    >
                        <span className="material-symbols-outlined">local_shipping</span>
                        Request Refill
                    </button>
                </div>
            </div>

            {/* Floating Action Buttons (Fixed above nav) */}
            <div className="absolute bottom-4 left-0 w-full px-4 z-30">
                <div className="flex gap-4">
                    <button
                        onClick={() => onScan('BIN')}
                        className="flex-1 flex flex-col items-center justify-center py-4 bg-white border-2 border-primary text-primary rounded-2xl font-black text-xs uppercase tracking-widest active:bg-slate-50 shadow-lg"
                    >
                        <span className="material-symbols-outlined mb-1 text-3xl">crop_free</span>
                        Scan Bin
                    </button>
                    <button
                        onClick={() => onScan('BUCKET')}
                        className="flex-1 flex flex-col items-center justify-center py-4 gradient-primary glow-primary text-white rounded-2xl font-black text-xs uppercase tracking-widest shadow-lg active:scale-95 transition-transform"
                    >
                        <span className="material-symbols-outlined mb-1 text-3xl">label</span>
                        Scan Sticker
                    </button>
                </div>
            </div>
        </div>
    );
};

export default LogisticsView;
