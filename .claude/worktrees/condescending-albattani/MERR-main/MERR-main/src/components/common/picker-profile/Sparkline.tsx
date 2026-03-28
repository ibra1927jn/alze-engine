/**
 * Sparkline — Mini SVG sparkline chart for picker daily data
 * Extracted from PickerProfileDrawer.tsx
 */
import React from 'react';

interface SparklineProps {
    data: number[];
    color: string;
    height?: number;
}

const Sparkline: React.FC<SparklineProps> = ({ data, color, height = 40 }) => {
    if (data.length < 2) return null;
    const max = Math.max(...data, 1);
    const w = 200;
    const points = data.map((v, i) => `${(i / (data.length - 1)) * w},${height - (v / max) * (height - 4)}`).join(' ');
    return (
        <svg viewBox={`0 0 ${w} ${height}`} className="w-full" preserveAspectRatio="none">
            <polyline fill="none" stroke={color} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" points={points} />
            <polyline fill={`${color}20`} stroke="none" points={`0,${height} ${points} ${w},${height}`} />
        </svg>
    );
};

export default Sparkline;
