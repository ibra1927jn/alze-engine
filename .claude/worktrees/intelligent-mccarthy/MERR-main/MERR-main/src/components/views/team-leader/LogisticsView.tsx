import React, { useMemo } from 'react';
import { HeatMapView } from '../manager/HeatMapView';
import ComponentErrorBoundary from '../@/components/ui/ComponentErrorBoundary';
import { useHarvestStore } from '@/stores/useHarvestStore';

const LogisticsView = () => {
    // 1. Conexión real al cerebro de datos
    const { bucketRecords, settings, orchard, rowAssignments } = useHarvestStore();

    // 2. Filtrar registros de hoy para el mapa
    const todayRecords = useMemo(() => {
        const startOfDay = new Date();
        startOfDay.setHours(0, 0, 0, 0);
        return bucketRecords.filter(r => new Date(r.scanned_at || '').getTime() >= startOfDay.getTime());
    }, [bucketRecords]);

    return (
        <div className="flex-1 flex flex-col w-full pb-32">
            <header className="sticky top-0 z-30 bg-surface-white/95 backdrop-blur-sm border-b border-border-light p-4 shadow-sm">
                <div className="flex items-center gap-3">
                    <div className="flex items-center justify-center size-10 rounded-full bg-white border border-primary-vibrant/20 text-primary-vibrant shadow-sm">
                        <span className="material-symbols-outlined text-[24px]">map</span>
                    </div>
                    <div>
                        <h1 className="text-text-main text-lg font-bold leading-tight">Orchard Map</h1>
                        <p className="text-xs text-text-sub font-medium">
                            {orchard?.name || 'Unknown Block'} • Real-time
                        </p>
                    </div>
                </div>
            </header>

            <main className="p-4 space-y-6">
                {/* 1. MAPA REAL */}
                <section>
                    <div className="flex justify-between items-end mb-4">
                        <h2 className="text-primary text-lg font-bold">Live Heatmap</h2>
                        <div className="flex gap-2">
                            <span className="text-xs font-medium text-text-sub bg-background-light px-2 py-1 rounded-md flex items-center gap-1">
                                <span className="size-2 rounded-full bg-success animate-pulse"></span>
                                Live
                            </span>
                        </div>
                    </div>
                    <div className="bg-white rounded-2xl border border-border-light shadow-sm overflow-hidden p-1 h-[300px]">
                        <div className="rounded-xl overflow-hidden border border-border-light h-full w-full relative">
                            <ComponentErrorBoundary componentName="Heat Map">
                                <HeatMapView />
                            </ComponentErrorBoundary>

                            {todayRecords.length === 0 && (
                                <div className="absolute inset-0 flex items-center justify-center bg-white/60 backdrop-blur-sm z-10">
                                    <div className="text-center">
                                        <span className="material-symbols-outlined text-4xl text-slate-300">grid_off</span>
                                        <p className="text-sm text-text-muted font-bold mt-2">No scans yet today</p>
                                    </div>
                                </div>
                            )}
                        </div>
                    </div>
                </section>

                {/* 2. TARIFAS REALES */}
                <section>
                    <h2 className="text-primary text-lg font-bold mb-4">Today's Rates</h2>
                    <div className="bg-gradient-to-br from-primary-vibrant to-primary-dim rounded-2xl p-5 text-white shadow-lg relative overflow-hidden">
                        <div className="flex justify-between items-start relative z-10">
                            <div>
                                <div className="flex items-center gap-1 text-white/80 text-xs font-bold uppercase tracking-wider mb-1">
                                    Min Wage Guarantee
                                </div>
                                <div className="text-3xl font-black tracking-tight">
                                    ${settings?.min_wage_rate?.toFixed(2) || '23.50'}
                                    <span className="text-sm font-medium text-white/70 ml-1">/ hr</span>
                                </div>
                            </div>
                            <div className="text-right">
                                <div className="text-xs text-white/80 font-bold uppercase tracking-wider mb-1">Piece Rate</div>
                                <div className="text-xl font-bold">
                                    ${settings?.piece_rate?.toFixed(2) || '6.50'}
                                    <span className="text-sm font-medium text-white/70 ml-1">/ bin</span>
                                </div>
                            </div>
                        </div>
                        <div className="absolute -right-6 -bottom-6 text-white/10 rotate-12 pointer-events-none">
                            <span className="material-symbols-outlined text-[100px]">payments</span>
                        </div>
                    </div>
                </section>

                {/* 3. RESUMEN DE FILAS ACTIVAS */}
                <section>
                    <div className="flex justify-between items-end mb-4">
                        <h2 className="text-primary text-lg font-bold">Active Rows</h2>
                    </div>
                    <div className="bg-white rounded-2xl border border-border-light shadow-sm overflow-hidden">
                        {rowAssignments && rowAssignments.length > 0 ? (
                            rowAssignments.slice(0, 5).map(assignment => (
                                <div key={assignment.id} className="p-4 border-b border-border-light last:border-0">
                                    <div className="flex justify-between items-center mb-2">
                                        <div className="flex items-center gap-2">
                                            <span className="size-6 bg-primary-vibrant text-white rounded flex items-center justify-center text-xs font-bold">
                                                {assignment.row_number}
                                            </span>
                                            <span className="text-xs font-bold text-text-sub uppercase">
                                                {assignment.side}
                                            </span>
                                        </div>
                                        <span className="text-[10px] font-bold bg-green-100 text-green-700 px-2 py-0.5 rounded-full">
                                            {assignment.assigned_pickers.length} Pickers
                                        </span>
                                    </div>
                                    <div className="w-full bg-slate-100 rounded-full h-1.5 overflow-hidden">
                                        <div
                                            className="bg-success h-1.5 rounded-full dynamic-width"
                                            style={{ '--w': `${assignment.completion_percentage || 5}%` } as React.CSSProperties}
                                        ></div>
                                    </div>
                                </div>
                            ))
                        ) : (
                            <div className="p-8 text-center text-text-muted">
                                <p className="text-sm font-medium">No rows assigned yet.</p>
                            </div>
                        )}
                    </div>
                </section>
            </main>
        </div>
    );
};

export default LogisticsView;
