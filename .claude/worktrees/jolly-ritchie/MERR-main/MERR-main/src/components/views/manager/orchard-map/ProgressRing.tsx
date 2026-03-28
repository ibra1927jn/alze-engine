/**
 * ProgressRing — SVG circular progress indicator
 */
import React from 'react';

interface ProgressRingProps {
    progress: number;
    size?: number;
    stroke?: number;
}

const ProgressRing: React.FC<ProgressRingProps> = ({ progress, size = 48, stroke = 4 }) => {
    const radius = (size - stroke) / 2;
    const circumference = 2 * Math.PI * radius;
    const offset = circumference * (1 - progress);
    const pct = Math.round(progress * 100);

    return (
        <svg width={size} height={size} className="transform -rotate-90">
            <circle cx={size / 2} cy={size / 2} r={radius} stroke="#e2e8f0" strokeWidth={stroke} fill="none" />
            <circle
                cx={size / 2} cy={size / 2} r={radius}
                stroke="url(#tacticalProgressGrad)" strokeWidth={stroke} fill="none"
                strokeLinecap="round"
                strokeDasharray={circumference}
                strokeDashoffset={offset}
                className="svg-stroke-transition"
            />
            <defs>
                <linearGradient id="tacticalProgressGrad" x1="0%" y1="0%" x2="100%" y2="0%">
                    <stop offset="0%" stopColor="#dc2626" />
                    <stop offset="100%" stopColor="#9333ea" />
                </linearGradient>
            </defs>
            <text
                x={size / 2} y={size / 2}
                textAnchor="middle" dominantBaseline="central"
                fill="#0f172a" fontSize={size > 40 ? 12 : 10} fontWeight={700}
                transform={`rotate(90, ${size / 2}, ${size / 2})`}
            >
                {pct}%
            </text>
        </svg>
    );
};

export default ProgressRing;
