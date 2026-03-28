/**
 * LOGISTICS DEPARTMENT — LogisticsDept.tsx
 * Uses DesktopLayout sidebar + modular tab views
 */
import React, { useState, useEffect } from 'react';
import DesktopLayout, { NavItem } from '@/components/common/DesktopLayout';
import { supabase } from '@/services/supabase';
import FleetTab from '@/components/views/logistics/FleetTab';
import BinsTab from '@/components/views/logistics/BinsTab';
import RequestsTab from '@/components/views/logistics/RequestsTab';
import RoutesTab from '@/components/views/logistics/RoutesTab';
import HistoryTab from '@/components/views/logistics/HistoryTab';
import LoadingSkeleton from '@/components/ui/LoadingSkeleton';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';
import {
    fetchLogisticsSummary, fetchFleet, fetchBinInventory,
    fetchTransportRequests, fetchTransportHistory,
    type LogisticsSummary, type Tractor, type BinInventory,
    type TransportRequest, type TransportLog
} from '@/services/logistics-dept.service';

const LOG_NAV_ITEMS: NavItem[] = [
    { id: 'fleet', label: 'Fleet', icon: 'agriculture' },
    { id: 'bins', label: 'Bin Inventory', icon: 'grid_view' },
    { id: 'requests', label: 'Requests', icon: 'swap_horiz' },
    { id: 'routes', label: 'Routes', icon: 'map' },
    { id: 'history', label: 'History', icon: 'history' },
];

const LogisticsDept: React.FC = () => {
    const [activeTab, setActiveTab] = useState('fleet');
    const [summary, setSummary] = useState<LogisticsSummary>({ fullBins: 0, emptyBins: 0, activeTractors: 0, pendingRequests: 0, binsInTransit: 0 });
    const [tractors, setTractors] = useState<Tractor[]>([]);
    const [bins, setBins] = useState<BinInventory[]>([]);
    const [requests, setRequests] = useState<TransportRequest[]>([]);
    const [history, setHistory] = useState<TransportLog[]>([]);
    const [isLoading, setIsLoading] = useState(true);

    const loadData = async () => {
        setIsLoading(true);
        const [sum, fleet, binData, reqs, hist] = await Promise.all([
            fetchLogisticsSummary(),
            fetchFleet(),
            fetchBinInventory(),
            fetchTransportRequests(),
            fetchTransportHistory(),
        ]);
        setSummary(sum);
        setTractors(fleet);
        setBins(binData);
        setRequests(reqs);
        setHistory(hist);
        setIsLoading(false);
    };

    useEffect(() => {
        loadData();

        // Supabase Realtime — auto-refresh on transport_requests & fleet_vehicles changes
        const channel = supabase
            .channel('logistics-realtime')
            .on('postgres_changes', { event: '*', schema: 'public', table: 'transport_requests' }, () => {
                // Silently reload requests + summary when any change occurs
                Promise.all([fetchTransportRequests(), fetchLogisticsSummary()]).then(([reqs, sum]) => {
                    setRequests(reqs);
                    setSummary(sum);
                });
            })
            .on('postgres_changes', { event: '*', schema: 'public', table: 'fleet_vehicles' }, () => {
                // Silently reload fleet + summary when any change occurs
                Promise.all([fetchFleet(), fetchLogisticsSummary()]).then(([fleet, sum]) => {
                    setTractors(fleet);
                    setSummary(sum);
                });
            })
            .subscribe();

        return () => { supabase.removeChannel(channel); };
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []);

    const navItems = LOG_NAV_ITEMS.map(item => ({
        ...item,
        badge: item.id === 'requests' ? summary.pendingRequests : undefined,
    }));

    if (isLoading) {
        return (
            <div className="min-h-screen bg-background-light p-6">
                <div className="max-w-6xl mx-auto space-y-4">
                    <div className="grid grid-cols-2 lg:grid-cols-3 gap-3">
                        <LoadingSkeleton type="metric" count={3} />
                    </div>
                    <LoadingSkeleton type="card" count={3} />
                </div>
            </div>
        );
    }

    const renderContent = () => {
        switch (activeTab) {
            case 'fleet': return <ComponentErrorBoundary componentName="Fleet"><FleetTab tractors={tractors} /></ComponentErrorBoundary>;
            case 'bins': return <ComponentErrorBoundary componentName="Bin Inventory"><BinsTab bins={bins} summary={summary} /></ComponentErrorBoundary>;
            case 'requests': return <ComponentErrorBoundary componentName="Requests"><RequestsTab requests={requests} tractors={tractors} onRefresh={loadData} /></ComponentErrorBoundary>;
            case 'routes': return <ComponentErrorBoundary componentName="Routes"><RoutesTab /></ComponentErrorBoundary>;
            case 'history': return <ComponentErrorBoundary componentName="History"><HistoryTab history={history} /></ComponentErrorBoundary>;
            default: return <ComponentErrorBoundary componentName="Fleet"><FleetTab tractors={tractors} /></ComponentErrorBoundary>;
        }
    };

    return (
        <DesktopLayout
            navItems={navItems}
            activeTab={activeTab}
            onTabChange={setActiveTab}
            title="Logistics"
            accentColor="teal"
            titleIcon="local_shipping"
        >
            {/* Live Status Banner */}
            <div className="bg-emerald-50 border border-emerald-200 rounded-xl px-4 py-3 flex items-center gap-3 mb-4">
                <span className="size-2.5 bg-emerald-500 rounded-full animate-pulse" />
                <p className="text-sm font-medium text-emerald-800">
                    {summary.activeTractors} Tractors Active • {summary.binsInTransit} Bins in Transit
                </p>
            </div>

            {/* Summary Cards */}
            <div className="grid grid-cols-2 lg:grid-cols-4 gap-3 mb-6">
                <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                    <div className="flex items-center gap-2 mb-1.5">
                        <span className="material-symbols-outlined text-red-500 text-lg">inventory_2</span>
                        <span className="text-xs text-text-secondary font-medium">Full Bins</span>
                    </div>
                    <p className="text-2xl font-black text-text-primary">{summary.fullBins}</p>
                    <div className="mt-2 h-1.5 bg-surface-secondary rounded-full overflow-hidden">
                        <div className="h-full logistics-progress-full rounded-full dynamic-width" style={{ '--w': `${Math.min(100, (summary.fullBins / (summary.fullBins + summary.emptyBins || 1)) * 100)}%` } as React.CSSProperties} />
                    </div>
                </div>
                <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                    <div className="flex items-center gap-2 mb-1.5">
                        <span className="material-symbols-outlined text-emerald-500 text-lg">check_box_outline_blank</span>
                        <span className="text-xs text-text-secondary font-medium">Empty Bins</span>
                    </div>
                    <p className="text-2xl font-black text-text-primary">{summary.emptyBins}</p>
                    <div className="mt-2 h-1.5 bg-surface-secondary rounded-full overflow-hidden">
                        <div className="h-full logistics-progress-empty rounded-full dynamic-width" style={{ '--w': `${Math.min(100, (summary.emptyBins / (summary.fullBins + summary.emptyBins || 1)) * 100)}%` } as React.CSSProperties} />
                    </div>
                </div>
                <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                    <div className="flex items-center gap-2 mb-1.5">
                        <span className="material-symbols-outlined text-indigo-500 text-lg">agriculture</span>
                        <span className="text-xs text-text-secondary font-medium">Active Tractors</span>
                    </div>
                    <p className="text-2xl font-black text-text-primary">{summary.activeTractors}</p>
                </div>
                <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                    <div className="flex items-center gap-2 mb-1.5">
                        <span className="material-symbols-outlined text-amber-500 text-lg">swap_horiz</span>
                        <span className="text-xs text-text-secondary font-medium">Transport Requests</span>
                    </div>
                    <p className="text-2xl font-black text-text-primary">{summary.pendingRequests}</p>
                </div>
            </div>

            {/* Tab Content */}
            <div key={activeTab} className="animate-fade-in">
                {renderContent()}
            </div>
        </DesktopLayout>
    );
};

export default LogisticsDept;
