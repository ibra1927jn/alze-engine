/**
 * Shared Settings Form Components
 * 
 * Reusable form primitives extracted from SettingsView.tsx
 * for use across settings panels throughout the app.
 */
import React from 'react';

/* ── FormField ──────────────────────────── */

interface FormFieldProps {
    label: string;
    value: string | number;
    onChange: (value: string | number) => void;
    type?: 'text' | 'number' | 'select';
    prefix?: string;
    suffix?: string;
    step?: string;
    options?: string[];
}

export const FormField: React.FC<FormFieldProps> = ({
    label, value, onChange, type = 'text', prefix, suffix, step, options,
}) => (
    <div className="flex flex-col gap-1">
        <label className="text-sm font-medium text-text-primary">{label}</label>
        <div className="flex items-center gap-2">
            {prefix && <span className="text-sm text-text-secondary font-medium">{prefix}</span>}
            {type === 'select' ? (
                <select
                    value={value}
                    onChange={(e) => onChange(e.target.value)}
                    aria-label={label}
                    className="flex-1 px-3 py-2 border border-border-medium rounded-md text-sm focus:ring-1 focus:ring-indigo-500 focus:border-indigo-500 outline-none"
                >
                    {options?.map(opt => (
                        <option key={opt} value={opt}>{opt}</option>
                    ))}
                </select>
            ) : (
                <input
                    type={type}
                    value={value}
                    step={step}
                    onChange={(e) => onChange(type === 'number' ? parseFloat(e.target.value) || 0 : e.target.value)}
                    aria-label={label}
                    className="flex-1 px-3 py-2 border border-border-medium rounded-md text-sm focus:ring-1 focus:ring-indigo-500 focus:border-indigo-500 outline-none"
                />
            )}
            {suffix && <span className="text-xs text-text-muted font-medium">{suffix}</span>}
        </div>
    </div>
);

/* ── ReadonlyField ──────────────────────── */

export const ReadonlyField: React.FC<{ label: string; value: string }> = ({ label, value }) => (
    <div className="flex flex-col gap-1">
        <label className="text-sm font-medium text-text-primary">{label}</label>
        <p className="px-3 py-2 bg-background-light border border-border-light rounded-md text-sm text-text-secondary">{value}</p>
    </div>
);

/* ── ToggleRow ──────────────────────────── */

interface ToggleRowProps {
    label: string;
    checked: boolean;
    onChange: (value: boolean) => void;
    locked?: boolean;
}

export const ToggleRow: React.FC<ToggleRowProps> = ({ label, checked, onChange, locked }) => (
    <div className="flex items-center justify-between py-1.5">
        <span className="text-sm text-text-primary font-medium flex items-center gap-2">
            {label}
            {locked && (
                <span className="text-[10px] bg-surface-secondary text-text-secondary px-1.5 py-0.5 rounded font-semibold">
                    REQUIRED
                </span>
            )}
        </span>
        <button
            onClick={() => !locked && onChange(!checked)}
            aria-label={`Toggle ${label}`}
            role="switch"
            aria-checked={checked ? "true" : "false"}
            className={`
                relative w-10 h-6 rounded-full transition-colors duration-200
                ${locked ? 'opacity-60 cursor-not-allowed' : 'cursor-pointer'}
                ${checked ? 'bg-green-500' : 'bg-surface-tertiary'}
            `}
            disabled={locked}
            type="button"
        >
            <span
                className={`
                    absolute top-1 left-1 w-4 h-4 bg-white rounded-full shadow transition-transform duration-200
                    ${checked ? 'translate-x-4' : 'translate-x-0'}
                `}
            />
        </button>
    </div>
);
