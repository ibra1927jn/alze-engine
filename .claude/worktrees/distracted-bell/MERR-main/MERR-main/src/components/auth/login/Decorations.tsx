/**
 * Login SVG Decorations — VineLeaf, GrapeCluster, ParticleDots
 * Pure presentational SVG components for the login hero panel.
 */
import React from 'react';

export const VineLeaf: React.FC<{ className?: string; style?: React.CSSProperties }> = ({ className = '', style }) => (
    <svg viewBox="0 0 60 60" fill="none" className={className} style={style}>
        <path d="M30 5C20 15 5 20 5 35c0 12 10 20 25 20s25-8 25-20C55 20 40 15 30 5z" fill="currentColor" opacity="0.15" />
        <path d="M30 5C30 20 22 28 15 35" stroke="currentColor" strokeWidth="1" opacity="0.2" fill="none" />
        <path d="M30 5C30 18 35 26 42 32" stroke="currentColor" strokeWidth="1" opacity="0.2" fill="none" />
    </svg>
);

export const GrapeCluster: React.FC<{ className?: string; style?: React.CSSProperties }> = ({ className = '', style }) => (
    <svg viewBox="0 0 40 50" fill="none" className={className} style={style}>
        <circle cx="14" cy="20" r="6" fill="currentColor" opacity="0.12" />
        <circle cx="26" cy="20" r="6" fill="currentColor" opacity="0.10" />
        <circle cx="20" cy="30" r="6" fill="currentColor" opacity="0.14" />
        <circle cx="10" cy="32" r="5" fill="currentColor" opacity="0.08" />
        <circle cx="30" cy="32" r="5" fill="currentColor" opacity="0.08" />
        <circle cx="20" cy="40" r="5" fill="currentColor" opacity="0.10" />
        <path d="M20 4C20 4 18 12 20 16" stroke="currentColor" strokeWidth="1.5" opacity="0.2" />
        <path d="M20 4C24 6 28 4 30 2" stroke="currentColor" strokeWidth="1" opacity="0.15" />
    </svg>
);

export const ParticleDots: React.FC = () => {
    const particles = Array.from({ length: 18 }, (_, i) => ({
        id: i,
        left: `${5 + Math.random() * 90}%`,
        top: `${5 + Math.random() * 90}%`,
        size: 2 + Math.random() * 3,
        duration: `${6 + Math.random() * 8}s`,
        delay: `${Math.random() * 5}s`,
        opacity: 0.15 + Math.random() * 0.25,
    }));

    return (
        <>
            {particles.map(p => (
                <div
                    key={p.id}
                    className="absolute rounded-full bg-lilac-glow login-vine-float pointer-events-none"
                    style={{
                        left: p.left, top: p.top,
                        width: p.size, height: p.size,
                        opacity: p.opacity,
                        '--duration': p.duration, '--delay': p.delay, '--start-rot': '0deg',
                    } as React.CSSProperties}
                />
            ))}
        </>
    );
};
