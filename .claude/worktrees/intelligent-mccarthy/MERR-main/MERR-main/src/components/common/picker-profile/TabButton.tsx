/**
 * TabButton — Pill-style tab switcher button
 * Extracted from PickerProfileDrawer.tsx
 */
import React from 'react';

interface TabButtonProps {
    active: boolean;
    label: string;
    onClick: () => void;
}

const TabButton: React.FC<TabButtonProps> = ({ active, label, onClick }) => (
    <button
        onClick={onClick}
        className={`px-4 py-1.5 rounded-full text-xs font-bold transition-all ${active
            ? 'gradient-primary text-white shadow-md'
            : 'text-text-muted hover:bg-slate-100'}`}
    >
        {label}
    </button>
);

export default TabButton;
