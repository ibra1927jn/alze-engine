/**
 * RiskBadge — Visual risk indicator for picker alerts
 * Extracted from PickerProfileDrawer.tsx
 */
import React from 'react';

interface RiskBadgeProps {
    badge: { type: string; severity: string; label: string; detail: string };
}

const icons: Record<string, string> = {
    fatigue: '🔋', chronic_topup: '💸', quality_drop: '📉', anomalous_scans: '🚨'
};
const bgColors: Record<string, string> = {
    warning: 'bg-amber-50 border-amber-200 text-amber-800',
    critical: 'bg-red-50 border-red-200 text-red-800',
};

const RiskBadge: React.FC<RiskBadgeProps> = ({ badge }) => (
    <div className={`flex items-center gap-2 px-3 py-2 rounded-xl border text-xs font-medium ${bgColors[badge.severity] || bgColors.warning}`}>
        <span className="text-base">{icons[badge.type] || '⚠️'}</span>
        <div>
            <p className="font-bold">{badge.label}</p>
            <p className="opacity-70 text-[10px]">{badge.detail}</p>
        </div>
    </div>
);

export default RiskBadge;
