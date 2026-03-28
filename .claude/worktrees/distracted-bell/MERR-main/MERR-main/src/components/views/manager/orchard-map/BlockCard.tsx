/**
 * BlockCard — Level 1 block card for OrchardMap macro view
 */
import React from 'react';
import { OrchardBlock } from '@/types';
import { BlockStats } from '@/hooks/useOrchardMap';
import {
    getBlockStatusColor, getBlockStatusBorder, getBlockTextColor,
    getStatusLabel, getVarietyStyle,
} from '@/utils/orchardMapUtils';

interface BlockCardProps {
    block: OrchardBlock;
    stats: BlockStats;
    varieties: string[];
    index: number;
    onClick: () => void;
}

const BlockCard: React.FC<BlockCardProps> = ({ block, stats, varieties, index, onClick }) => {
    const statusInfo = getStatusLabel(block.status);
    const textColor = getBlockTextColor(block.status);

    return (
        <button
            onClick={onClick}
            className={`
                relative rounded-2xl p-6 text-left
                transition-all duration-300 ease-out
                animate-slide-up anim-delay
                hover:scale-[1.02] hover:shadow-xl active:scale-[0.98] cursor-pointer shadow-md
                focus:outline-none focus:ring-2 focus:ring-primary/50 focus:ring-offset-2
                min-h-[120px] block-card-dynamic
            `}
            style={{
                '--block-bg': getBlockStatusColor(block.status),
                '--block-border': getBlockStatusBorder(block.status),
                '--block-text': textColor,
                '--delay': `${index * 0.08}s`,
            } as React.CSSProperties}
        >
            {/* Header */}
            <div className="flex items-start justify-between mb-3">
                <div>
                    <h3 className="text-xl font-black dynamic-text-color">{block.name}</h3>
                    <div className="flex flex-wrap items-center gap-1.5 mt-1.5">
                        {varieties.map(v => {
                            const vs = getVarietyStyle(v);
                            return (
                                <span key={v} className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-[10px] font-bold variety-badge" style={{ '--variety-bg': vs.bg, '--variety-text': vs.text, '--variety-dot': vs.dot } as React.CSSProperties}>
                                    <span className="w-1.5 h-1.5 rounded-full variety-dot" />
                                    {v}
                                </span>
                            );
                        })}
                    </div>
                </div>
                <div className={`
                    flex items-center gap-1.5 px-3 py-1.5 rounded-full text-xs font-bold shrink-0
                    ${block.status === 'active' ? 'bg-amber-100 text-amber-800' : ''}
                    ${block.status === 'idle' ? 'bg-slate-100 text-slate-600' : ''}
                    ${block.status === 'complete' ? 'bg-white/20 text-white' : ''}
                    ${block.status === 'alert' ? 'bg-white/20 text-white animate-breathe' : ''}
                `}>
                    <span className="material-symbols-outlined text-sm">{statusInfo.icon}</span>
                    {statusInfo.label}
                </div>
            </div>

            {/* Stats */}
            <div className="grid grid-cols-3 gap-3">
                {[
                    { value: block.totalRows, label: 'Rows' },
                    { value: stats.activePickers, label: 'Pickers' },
                    { value: `🍒 ${stats.buckets}`, label: 'Buckets' },
                ].map(stat => (
                    <div key={stat.label} className="bg-white/30 backdrop-blur-sm rounded-xl px-3 py-2 text-center">
                        <div className="text-lg font-black dynamic-text-color">{stat.value}</div>
                        <div className="text-[10px] font-semibold uppercase tracking-wider dynamic-text-color opacity-70">{stat.label}</div>
                    </div>
                ))}
            </div>

            {/* Progress bar */}
            <div className="mt-4">
                <div className="flex justify-between text-[10px] font-bold mb-1 dynamic-text-color opacity-80">
                    <span>Progress</span>
                    <span>{stats.completedRows}/{block.totalRows} rows</span>
                </div>
                <div className="h-2 rounded-full bg-black/10 overflow-hidden">
                    <div
                        className="h-full rounded-full bg-white/60 animate-progress dynamic-width"
                        style={{ '--w': `${Math.round(stats.progress * 100)}%` } as React.CSSProperties}
                    />
                </div>
            </div>

            {/* Drill-in hint */}
            <div className="mt-3 flex items-center justify-end gap-1 text-xs font-semibold dynamic-text-color opacity-60">
                <span>View Rows</span>
                <span className="material-symbols-outlined text-sm">chevron_right</span>
            </div>
        </button>
    );
};

export default BlockCard;
