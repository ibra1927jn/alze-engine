import React from 'react';
import { useHarvestStore as useHarvest } from '@/stores/useHarvestStore';

interface RunnersViewProps {
    onBack: () => void;
}

const RunnersView: React.FC<RunnersViewProps> = ({ onBack }) => {
    const { crew, orchard } = useHarvest();

    const activePickers = crew;

    return (
        <div className="flex flex-col h-full bg-background-light">
            <header className="pt-8 pb-4 px-5 flex items-center justify-between z-20 bg-white/90 backdrop-blur-md sticky top-0 border-b border-border-light shadow-[0_4px_20px_-10px_rgba(0,0,0,0.05)]">
                <button
                    onClick={onBack}
                    className="size-10 flex items-center justify-center rounded-full bg-white border border-border-light shadow-sm text-text-sub active:bg-slate-50 transition-colors"
                >
                    <span className="material-symbols-outlined">arrow_back</span>
                </button>
                <div className="text-center">
                    <h1 className="text-lg font-black text-text-main uppercase tracking-wide">Orchard Crew</h1>
                    <p className="text-[10px] text-primary font-bold tracking-widest uppercase">{orchard?.name || 'Live Ops'}</p>
                </div>
                <div className="size-10"></div> {/* Spacer for symmetry */}
            </header>

            <div className="flex-1 overflow-y-auto px-4 py-6 space-y-4 z-10 pb-20">
                <div className="flex items-center justify-between px-1 mb-2">
                    <h2 className="text-sm font-bold text-text-muted uppercase tracking-wide">Active Pickers</h2>
                    <div className="flex items-center gap-2">
                        <span className="relative flex h-2.5 w-2.5">
                            <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
                            <span className="relative inline-flex rounded-full h-2.5 w-2.5 bg-emerald-500"></span>
                        </span>
                        <span className="text-primary font-bold text-xs uppercase tracking-wider">{activePickers.length} Checked In</span>
                    </div>
                </div>

                {activePickers.map(picker => (
                    <div key={picker.id} className="bg-white p-5 rounded-2xl shadow-[0_2px_12px_rgba(0,0,0,0.06)] border border-border-light flex flex-col gap-4">
                        <div className="flex items-start justify-between">
                            <div className="flex items-center gap-4">
                                <div className="size-14 rounded-2xl bg-slate-100 flex items-center justify-center text-lg font-black text-text-muted border border-border-light uppercase">
                                    {picker.avatar}
                                </div>
                                <div>
                                    <h3 className="font-bold text-text-main text-xl leading-tight">{picker.name}</h3>
                                    <div className="flex items-center gap-1.5 mt-1">
                                        <span className="material-symbols-outlined text-sm text-primary filled">location_on</span>
                                        <span className="text-sm font-medium text-text-sub">Row {picker.current_row || '?'}</span>
                                    </div>
                                </div>
                            </div>
                            <div className="flex flex-col items-end gap-1">
                                <div className="size-3 rounded-full bg-emerald-500 shadow-[0_0_0_4px_rgba(34,197,94,0.15)]"></div>
                            </div>
                        </div>
                        <div className="bg-slate-50 rounded-xl p-3 border border-border-light flex items-center justify-between">
                            <div>
                                <p className="text-[10px] text-text-muted font-bold uppercase tracking-wider mb-0.5">Buckets</p>
                                <p className="text-sm font-semibold text-text-main">{picker.total_buckets_today || 0}</p>
                            </div>
                            <span className="material-symbols-outlined text-text-muted">shopping_basket</span>
                        </div>
                    </div>
                ))}

                {activePickers.length === 0 && (
                    <div className="text-center py-10 opacity-50">
                        <p>No active pickers found.</p>
                    </div>
                )}
            </div>
        </div>
    );
};

export default RunnersView;
