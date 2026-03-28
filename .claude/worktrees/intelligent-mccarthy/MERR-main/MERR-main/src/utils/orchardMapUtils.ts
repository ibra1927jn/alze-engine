/**
 * orchardMapUtils — Color engine, status labels, and variety styling
 * Pure utilities with zero React dependencies.
 */
import { OrchardBlock } from '@/types';

/* ── Block Status Colors ─────────────────── */

export function getBlockStatusColor(status: OrchardBlock['status']): string {
    switch (status) {
        case 'idle': return 'linear-gradient(135deg, #f1f5f9 0%, #e2e8f0 100%)';
        case 'active': return 'linear-gradient(135deg, #fef3c7 0%, #fbbf24 100%)';
        case 'complete': return 'linear-gradient(135deg, #a7f3d0 0%, #10b981 100%)';
        case 'alert': return 'linear-gradient(135deg, #fecaca 0%, #ef4444 100%)';
    }
}

export function getBlockStatusBorder(status: OrchardBlock['status']): string {
    switch (status) {
        case 'idle': return '#cbd5e1';
        case 'active': return '#f59e0b';
        case 'complete': return '#10b981';
        case 'alert': return '#ef4444';
    }
}

export function getBlockTextColor(status: OrchardBlock['status']): string {
    return status === 'complete' || status === 'alert' ? '#ffffff' : '#1e293b';
}

export function getStatusLabel(status: OrchardBlock['status']): { label: string; icon: string } {
    switch (status) {
        case 'idle': return { label: 'Unassigned', icon: 'hourglass_empty' };
        case 'active': return { label: 'In Progress', icon: 'trending_up' };
        case 'complete': return { label: 'Complete', icon: 'check_circle' };
        case 'alert': return { label: 'Alert', icon: 'warning' };
    }
}

/* ── Row Progress ────────────────────────── */

export function getRowProgress(buckets: number, target: number): number {
    return target > 0 ? Math.min(buckets / target, 1) : 0;
}

export function getRowGradient(progress: number): string {
    if (progress <= 0) return 'linear-gradient(135deg, #f8fafc, #f1f5f9)';
    if (progress < 0.25) return 'linear-gradient(135deg, #fef9c3, #fde68a)';
    if (progress < 0.5) return 'linear-gradient(135deg, #fde68a, #fbbf24)';
    if (progress < 0.75) return 'linear-gradient(135deg, #bbf7d0, #4ade80)';
    if (progress < 1) return 'linear-gradient(135deg, #6ee7b7, #10b981)';
    return 'linear-gradient(135deg, #10b981, #059669)';
}

/* ── Avatar Colors ───────────────────────── */

export const AVATAR_COLORS = [
    '#6366f1', '#8b5cf6', '#ec4899', '#f43f5e',
    '#0ea5e9', '#14b8a6', '#f59e0b', '#84cc16',
];

/* ── Cherry Variety Colors ───────────────── */

export interface VarietyStyle {
    bg: string;
    text: string;
    dot: string;
}

const VARIETY_COLORS: Record<string, VarietyStyle> = {
    'Lapins': { bg: '#fef2f2', text: '#991b1b', dot: '#dc2626' },
    'Sweetheart': { bg: '#fdf2f8', text: '#9d174d', dot: '#ec4899' },
    'Bing': { bg: '#faf5ff', text: '#6b21a8', dot: '#9333ea' },
    'Rainier': { bg: '#fefce8', text: '#854d0e', dot: '#eab308' },
    'Stella': { bg: '#f0fdf4', text: '#166534', dot: '#16a34a' },
    'Kordia': { bg: '#fff7ed', text: '#9a3412', dot: '#ea580c' },
};

export function getVarietyStyle(variety: string): VarietyStyle {
    return VARIETY_COLORS[variety] || { bg: '#f8fafc', text: '#475569', dot: '#94a3b8' };
}
