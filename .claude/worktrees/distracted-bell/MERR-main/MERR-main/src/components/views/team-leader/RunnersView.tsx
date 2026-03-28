import React, { useMemo } from 'react';
import { useHarvestStore as useHarvest } from '@/stores/useHarvestStore';

// Eliminamos las props simuladas
const RunnersView = () => {
    const { crew } = useHarvest();

    // 1. Encontrar Runners Reales en la cuadrilla
    const realRunners = useMemo(() => {
        return crew.filter(p =>
            p.role === 'Runner' ||
            p.role?.toLowerCase() === 'runner'
        );
    }, [crew]);

    return (
        <div className="flex-1 flex flex-col w-full pb-32">
            <header className="sticky top-0 z-30 bg-surface-white/95 backdrop-blur-sm border-b border-border-light p-4 shadow-sm">
                <div className="flex items-center gap-3">
                    <div className="flex items-center justify-center size-10 rounded-full bg-white border border-primary-vibrant/20 text-primary-vibrant shadow-sm">
                        <span className="material-symbols-outlined text-[24px]">local_shipping</span>
                    </div>
                    <div>
                        <h1 className="text-text-main text-lg font-bold leading-tight">Runner Logistics</h1>
                        <p className="text-xs text-text-sub font-medium">
                            {realRunners.length} Active Runners
                        </p>
                    </div>
                </div>
            </header>

            <main className="p-4">
                {realRunners.length === 0 ? (
                    <div className="bg-white rounded-2xl p-8 text-center border border-dashed border-border-light mt-4">
                        <span className="material-symbols-outlined text-slate-300 text-5xl mb-2">person_off</span>
                        <h3 className="text-lg font-bold text-text-sub">No Runners Found</h3>
                        <p className="text-sm text-text-muted mt-1">
                            Add a member with role "Runner" in the Team tab.
                        </p>
                    </div>
                ) : (
                    <div className="grid gap-4">
                        {realRunners.map(runner => (
                            <div key={runner.id} className="bg-white rounded-xl p-4 border border-border-light shadow-sm flex items-center justify-between">
                                <div className="flex items-center gap-3">
                                    <div className="size-12 rounded-full bg-purple-100 text-purple-600 flex items-center justify-center font-bold border border-purple-200">
                                        {runner.avatar || runner.name.substring(0, 2).toUpperCase()}
                                    </div>
                                    <div>
                                        <h3 className="font-bold text-text-main">{runner.name}</h3>
                                        <div className="flex items-center gap-2 text-xs text-text-sub">
                                            <span className="bg-purple-50 text-purple-700 px-1.5 py-0.5 rounded font-bold uppercase tracking-wider text-[10px]">Runner</span>
                                            <span>• ID: {runner.picker_id}</span>
                                        </div>
                                    </div>
                                </div>

                                <div className="text-right">
                                    <div className="flex flex-col items-end">
                                        <span className={`text-[10px] font-bold px-2 py-0.5 rounded-full mb-1 ${runner.status === 'active' ? 'bg-green-100 text-green-700' : 'bg-slate-100 text-text-muted'
                                            }`}>
                                            {runner.status?.toUpperCase() || 'OFFLINE'}
                                        </span>
                                    </div>
                                </div>
                            </div>
                        ))}
                    </div>
                )}
            </main>
        </div>
    );
};

export default RunnersView;
