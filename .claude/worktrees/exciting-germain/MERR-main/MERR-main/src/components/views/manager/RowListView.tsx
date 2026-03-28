import React from 'react';
import { Picker, Tab } from '../../../types';

interface RowListViewProps {
    runners: Picker[];
    setActiveTab: (tab: Tab) => void;
    onRowClick?: (rowNum: number) => void;
    // NUEVAS PROPS DE DATOS
    blockName?: string;
    totalRows?: number;
    variety?: string;
    targetYield?: number;
}

const RowListView: React.FC<RowListViewProps> = ({
    runners,
    onRowClick,
    blockName = "Unknown Block",
    totalRows = 20,
    variety = "Mix",
    targetYield = 1000
}) => {
    // 1. GENERACIÓN DINÁMICA DE FILAS (Clave para que funcione MP3 vs MP4)
    // Si totalRows viene como 0 o null, usamos 1 por seguridad
    const safeRowCount = Math.max(1, totalRows || 20);
    const rows = Array.from({ length: safeRowCount }, (_, i) => i + 1);

    // Agrupar runners por fila para cálculos rápidos
    const runnersByRow = runners.reduce((acc, runner) => {
        const r = parseInt(runner.current_row?.toString() || '0');
        if (r > 0) {
            if (!acc[r]) acc[r] = [];
            acc[r].push(runner);
        }
        return acc;
    }, {} as Record<number, typeof runners>);

    // Helpers de cálculo
    const getBucketsForRow = (r: number) => {
        const rowRunners = runnersByRow[r] || [];
        if (rowRunners.length === 0) return 0;
        return rowRunners.reduce((sum, runner) => sum + (runner.total_buckets_today || 0), 0);
    };

    const getProgress = (buckets: number) => Math.min(100, Math.round((buckets / 150) * 100)); // Target estimado por fila

    const calculateETA = (buckets: number) => {
        if (buckets === 0) return '--:--';
        const remaining = Math.max(0, 150 - buckets);
        // Simulación: 8 cubos/hora por persona (o velocity estimadad)
        const velocity = 10;
        const minutes = Math.ceil((remaining / velocity) * 60);
        const h = Math.floor(minutes / 60);
        const m = minutes % 60;
        return `${h}:${String(m).padStart(2, '0')}`;
    };

    // Estadísticas Generales
    const totalActivePickers = runners.filter(r => r.current_row > 0).length;
    const totalYield = rows.reduce((sum, r) => sum + getBucketsForRow(r), 0);
    const yieldPercentage = Math.min(100, (totalYield / targetYield) * 100);

    return (
        <div className="flex flex-col h-full bg-white text-text-sub font-mono relative overflow-hidden">
            {/* Background Grid */}
            <div className="absolute inset-0 technical-grid z-0"></div>

            {/* HEADER DINÁMICO */}
            <header className="z-50 px-4 pt-6 pb-4 flex flex-col gap-4 bg-white/90 backdrop-blur-sm border-b border-border-light relative">
                <div className="flex justify-between items-center">
                    <div className="flex items-center gap-3">
                        <div className="p-2 border border-primary/30 bg-primary/5 rounded">
                            <span className="material-symbols-outlined text-primary text-xl">grid_view</span>
                        </div>
                        <div>
                            <h1 className="text-sm font-bold tracking-widest text-text-main uppercase flex items-center gap-2">
                                {blockName} <span className="text-primary">{variety}</span>
                            </h1>
                            <p className="text-[9px] text-text-muted uppercase tracking-tighter">
                                Live Control • {safeRowCount} Rows
                            </p>
                        </div>
                    </div>
                    <div className="flex flex-col items-end">
                        <div className="text-[10px] text-primary flex items-center gap-1.5 px-2 py-0.5 border border-primary/20 rounded bg-primary/5">
                            <span className="w-1.5 h-1.5 bg-primary rounded-full animate-pulse shadow-[0_0_5px_var(--color-primary)]"></span>
                            LIVE
                        </div>
                        <p className="text-[8px] text-text-muted mt-1 uppercase">Lat: -41.2865 | Lon 174.7762</p>
                    </div>
                </div>

                <div className="grid grid-cols-2 gap-2">
                    <div className="bg-slate-50 p-2 border border-border-light rounded-sm">
                        <div className="text-[8px] text-text-muted uppercase">Pickers Online</div>
                        <div className="text-lg font-bold text-text-main leading-tight">{totalActivePickers}</div>
                    </div>
                    <div className="bg-slate-50 p-2 border border-border-light rounded-sm">
                        <div className="text-[8px] text-text-muted uppercase">Row Count</div>
                        <div className="text-lg font-bold text-text-main leading-tight">{safeRowCount}</div>
                    </div>
                </div>

                <div className="space-y-1.5">
                    <div className="flex justify-between text-[10px] uppercase tracking-wider">
                        <span className="text-slate-400">Total Yield</span>
                        <span className="text-primary">{totalYield} / {targetYield} BUCKETS</span>
                        {/* Note: changed 'UNITS' to 'BUCKETS' in user request but let's stick to consistent terminology or follow request exactly. The request said BUCKETS in the header replacement description but UNITS in the first request. The user provided explicit code for replacement which says BUCKETS. I will follow the user provided code block.*/}
                    </div>
                    <div className="relative w-full h-1.5 bg-slate-100 border border-border-light rounded-full overflow-hidden">
                        <div
                            className="h-full dynamic-width bg-primary relative z-10 shadow-[0_0_8px_rgba(99,102,241,0.4)] transition-all duration-700"
                            style={{ '--w': `${yieldPercentage}%` } as React.CSSProperties}
                        ></div>
                    </div>
                </div>
            </header>

            {/* MAIN LIST */}
            <main className="flex-1 overflow-y-auto custom-scrollbar px-4 py-4 space-y-2 relative z-10 pb-24">
                {/* Column Headers */}
                <div className="grid grid-cols-12 px-3 text-[8px] text-text-muted uppercase tracking-widest mb-1">
                    <div className="col-span-2">ID</div>
                    <div className="col-span-2">Status</div>
                    <div className="col-span-6">Harvest Progress</div>
                    <div className="col-span-1 text-right">Units</div>
                    <div className="col-span-1 text-right">ETA</div>
                </div>

                {/* Rows Loop */}
                {rows.map(rowNum => {
                    const buckets = getBucketsForRow(rowNum);
                    const isActive = runnersByRow[rowNum]?.length > 0;
                    const progress = getProgress(buckets);
                    const rowId = `R${String(rowNum).padStart(2, '0')}`;

                    return (
                        <div
                            key={rowNum}
                            onClick={() => onRowClick && onRowClick(rowNum)}
                            className={`row-card p-3 rounded-xl grid grid-cols-12 items-center cursor-pointer transition-all hover:shadow-md ${isActive ? 'border-l-3 border-l-primary bg-white' : 'opacity-60 hover:opacity-100'}`}
                        >
                            <div className={`col-span-2 font-bold text-xs tracking-tighter ${isActive ? 'text-text-main' : 'text-text-muted'}`}>
                                {rowId}
                            </div>

                            <div className="col-span-2 flex items-center gap-1.5">
                                <div className={`status-dot ${isActive ? 'bg-primary shadow-[0_0_4px_var(--color-primary)]' : 'bg-slate-300'}`}></div>
                                <span className={`text-[9px] uppercase font-medium ${isActive ? 'text-primary' : 'text-text-muted'}`}>
                                    {isActive ? 'Active' : 'Idle'}
                                </span>
                            </div>

                            <div className="col-span-6 px-2">
                                <div className="w-full bg-slate-100 h-1 relative border border-border-light rounded-full overflow-hidden">
                                    <div
                                        className={`h-full transition-all duration-500 dynamic-width ${isActive ? 'bg-primary shadow-[0_0_5px_rgba(99,102,241,0.3)]' : 'bg-slate-200'}`}
                                        style={{ '--w': `${progress}%` } as React.CSSProperties}
                                    ></div>
                                </div>
                            </div>

                            <div className={`col-span-1 text-right text-[10px] font-mono ${isActive ? 'text-text-sub' : 'text-text-muted'}`}>
                                {buckets}
                            </div>

                            <div className={`col-span-1 text-right text-[10px] font-mono ${isActive ? 'text-text-muted' : 'text-text-muted'}`}>
                                {isActive ? calculateETA(buckets) : '--:--'}
                            </div>
                        </div>
                    );
                })}
            </main>
        </div>
    );
};

export default RowListView;
