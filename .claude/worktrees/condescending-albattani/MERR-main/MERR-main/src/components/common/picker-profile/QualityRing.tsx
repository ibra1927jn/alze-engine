/**
 * QualityRing — Circular progress ring for quality score
 * Extracted from PickerProfileDrawer.tsx
 */
import React from 'react';

interface QualityRingProps {
    score: number;
}

const QualityRing: React.FC<QualityRingProps> = ({ score }) => {
    const pct = Math.min(100, score);
    const circumference = 2 * Math.PI * 16;
    const strokeOffset = circumference - (pct / 100) * circumference;
    const color = score >= 70 ? '#10b981' : score >= 40 ? '#f59e0b' : '#ef4444';

    return (
        <div className="relative w-14 h-14">
            <svg viewBox="0 0 36 36" className="w-full h-full transform -rotate-90">
                <circle cx="18" cy="18" r="16" fill="none" stroke="#e2e8f0" strokeWidth="2.5" />
                <circle cx="18" cy="18" r="16" fill="none" stroke={color} strokeWidth="2.5"
                    strokeLinecap="round" strokeDasharray={circumference} strokeDashoffset={strokeOffset}
                    className="transition-all duration-1000" />
            </svg>
            <div className="absolute inset-0 flex items-center justify-center">
                <span className="text-xs font-black dynamic-text-color" style={{ '--text-color': color } as React.CSSProperties}>{score}</span>
            </div>
        </div>
    );
};

export default QualityRing;
