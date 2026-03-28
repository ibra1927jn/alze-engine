/**
 * SetupWizard.tsx — Guided multi-step orchard setup wizard
 *
 * 4 steps: Create Orchard → Set Up Teams → Configure Rates → Summary
 * Used for first-time setup or adding new orchards from Admin panel.
 */
import { logger } from '@/utils/logger';
import React, { useState } from 'react';
import { createOrchardSetup, type OrchardSetupData } from '@/services/setup.service';

/* ── Types ────────────────── */
interface WizardData {
    orchard: { code: string; name: string; location: string; total_rows: number; };
    teams: { name: string; leader_name: string; max_pickers: number; }[];
    rates: { variety: string; piece_rate: number; start_time: string; };
}

interface SetupWizardProps {
    onComplete: () => void;
    onCancel: () => void;
}

const STEPS = [
    { key: 'orchard', label: 'Create Orchard', icon: 'park' },
    { key: 'teams', label: 'Set Up Teams', icon: 'groups' },
    { key: 'rates', label: 'Configure Rates', icon: 'payments' },
    { key: 'summary', label: 'Review & Confirm', icon: 'check_circle' },
] as const;

const INITIAL_DATA: WizardData = {
    orchard: { code: '', name: '', location: '', total_rows: 20 },
    teams: [{ name: 'Team Alpha', leader_name: '', max_pickers: 15 }],
    rates: { variety: 'Lapin', piece_rate: 1.80, start_time: '06:30' },
};

const VARIETIES = ['Lapin', 'Sweetheart', 'Kordia', 'Stella', 'Rainier', 'Skeena', 'Bing', 'Brooks', 'Staccato', 'Sam', 'White Gold', 'Earlise'];

export default function SetupWizard({ onComplete, onCancel }: SetupWizardProps) {
    const [step, setStep] = useState(0);
    const [data, setData] = useState<WizardData>({ ...INITIAL_DATA });
    const [isSubmitting, setIsSubmitting] = useState(false);
    const [error, setError] = useState<string | null>(null);

    const updateOrchard = (field: keyof WizardData['orchard'], value: string | number) => {
        setData(prev => ({ ...prev, orchard: { ...prev.orchard, [field]: value } }));
    };

    const updateTeam = (idx: number, field: keyof WizardData['teams'][0], value: string | number) => {
        setData(prev => {
            const teams = [...prev.teams];
            teams[idx] = { ...teams[idx], [field]: value };
            return { ...prev, teams };
        });
    };

    const addTeam = () => {
        setData(prev => ({
            ...prev,
            teams: [...prev.teams, { name: `Team ${String.fromCharCode(65 + prev.teams.length)}`, leader_name: '', max_pickers: 15 }],
        }));
    };

    const removeTeam = (idx: number) => {
        if (data.teams.length <= 1) return;
        setData(prev => ({ ...prev, teams: prev.teams.filter((_, i) => i !== idx) }));
    };

    const updateRates = (field: keyof WizardData['rates'], value: string | number) => {
        setData(prev => ({ ...prev, rates: { ...prev.rates, [field]: value } }));
    };

    const canProceed = (): boolean => {
        switch (step) {
            case 0: return !!(data.orchard.code.trim() && data.orchard.name.trim());
            case 1: return data.teams.every(t => t.name.trim().length > 0);
            case 2: return data.rates.piece_rate > 0;
            default: return true;
        }
    };

    const handleSubmit = async () => {
        setIsSubmitting(true);
        setError(null);

        const setupData: OrchardSetupData = {
            orchard: data.orchard,
            teams: data.teams,
            rates: data.rates,
        };

        const result = await createOrchardSetup(setupData);

        if (result.ok === false) {
            const { error: svcError } = result;
            logger.error('[Wizard] Setup failed:', svcError.toString());
            setError(svcError.message);
            setIsSubmitting(false);
            return;
        }

        logger.info('[Wizard] Orchard created:', result.data.code);
        setIsSubmitting(false);
        onComplete();
    };

    return (
        <div className="fixed inset-0 z-50 bg-black/50 flex items-center justify-center p-4">
            <div className="bg-white rounded-2xl shadow-2xl w-full max-w-2xl max-h-[90vh] overflow-hidden flex flex-col animate-in fade-in zoom-in-95">

                {/* Header */}
                <div className="px-6 py-4 border-b border-border-light flex items-center justify-between">
                    <div className="flex items-center gap-3">
                        <div className="w-10 h-10 bg-emerald-50 rounded-lg flex items-center justify-center">
                            <span className="material-symbols-outlined text-xl text-emerald-600">rocket_launch</span>
                        </div>
                        <div>
                            <h2 className="text-lg font-semibold text-text-primary">New Orchard Setup</h2>
                            <p className="text-xs text-text-secondary">Step {step + 1} of {STEPS.length}</p>
                        </div>
                    </div>
                    <button onClick={onCancel} className="text-text-muted hover:text-text-secondary" aria-label="Close wizard">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                {/* Stepper */}
                <div className="px-6 pt-4">
                    <div className="flex items-center gap-1">
                        {STEPS.map((s, i) => (
                            <React.Fragment key={s.key}>
                                <div className={`flex items-center gap-1.5 px-2 py-1 rounded-full text-xs font-medium transition-colors ${i === step ? 'bg-emerald-100 text-emerald-700' :
                                    i < step ? 'bg-emerald-50 text-emerald-600' :
                                        'bg-background-light text-text-muted'
                                    }`}>
                                    <span className="material-symbols-outlined text-sm">
                                        {i < step ? 'check_circle' : s.icon}
                                    </span>
                                    <span className="hidden sm:inline">{s.label}</span>
                                </div>
                                {i < STEPS.length - 1 && (
                                    <div className={`flex-1 h-0.5 rounded ${i < step ? 'bg-emerald-300' : 'bg-surface-secondary'}`} />
                                )}
                            </React.Fragment>
                        ))}
                    </div>
                </div>

                {/* Content */}
                <div className="flex-1 overflow-y-auto p-6 space-y-4">
                    {/* Step 0: Orchard */}
                    {step === 0 && (
                        <div className="space-y-4">
                            <div className="grid grid-cols-2 gap-4">
                                <div>
                                    <label className="block text-sm font-medium text-text-primary mb-1">Orchard Code *</label>
                                    <input
                                        type="text"
                                        value={data.orchard.code}
                                        onChange={e => updateOrchard('code', e.target.value.toUpperCase())}
                                        placeholder="e.g. JP-01"
                                        className="w-full border border-border-light rounded-lg px-3 py-2 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                    />
                                </div>
                                <div>
                                    <label className="block text-sm font-medium text-text-primary mb-1">Orchard Name *</label>
                                    <input
                                        type="text"
                                        value={data.orchard.name}
                                        onChange={e => updateOrchard('name', e.target.value)}
                                        placeholder="e.g. J&P Cherries"
                                        className="w-full border border-border-light rounded-lg px-3 py-2 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                    />
                                </div>
                            </div>
                            <div>
                                <label className="block text-sm font-medium text-text-primary mb-1">Location</label>
                                <input
                                    type="text"
                                    value={data.orchard.location}
                                    onChange={e => updateOrchard('location', e.target.value)}
                                    placeholder="e.g. Cromwell, Central Otago"
                                    className="w-full border border-border-light rounded-lg px-3 py-2 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                />
                            </div>
                            <div>
                                <label htmlFor="wizard-total-rows" className="block text-sm font-medium text-text-primary mb-1">Total Rows</label>
                                <input
                                    id="wizard-total-rows"
                                    type="number"
                                    min={1}
                                    max={500}
                                    value={data.orchard.total_rows}
                                    onChange={e => updateOrchard('total_rows', parseInt(e.target.value) || 1)}
                                    className="w-32 border border-border-light rounded-lg px-3 py-2 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                />
                            </div>
                        </div>
                    )}

                    {/* Step 1: Teams */}
                    {step === 1 && (
                        <div className="space-y-4">
                            <p className="text-sm text-text-secondary">Define your picking teams. You can add more teams later.</p>
                            {data.teams.map((team, idx) => (
                                <div key={idx} className="bg-background-light rounded-lg p-4 space-y-3">
                                    <div className="flex items-center justify-between">
                                        <h4 className="text-sm font-semibold text-text-primary">Team {idx + 1}</h4>
                                        {data.teams.length > 1 && (
                                            <button onClick={() => removeTeam(idx)} className="text-xs text-red-500 hover:text-red-700">
                                                Remove
                                            </button>
                                        )}
                                    </div>
                                    <div className="grid grid-cols-3 gap-3">
                                        <div>
                                            <label htmlFor={`wizard-team-name-${idx}`} className="block text-xs text-text-secondary mb-1">Team Name</label>
                                            <input
                                                id={`wizard-team-name-${idx}`}
                                                type="text"
                                                value={team.name}
                                                onChange={e => updateTeam(idx, 'name', e.target.value)}
                                                className="w-full border border-border-light rounded-lg px-2 py-1.5 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                            />
                                        </div>
                                        <div>
                                            <label className="block text-xs text-text-secondary mb-1">Team Leader</label>
                                            <input
                                                type="text"
                                                value={team.leader_name}
                                                onChange={e => updateTeam(idx, 'leader_name', e.target.value)}
                                                placeholder="Optional"
                                                className="w-full border border-border-light rounded-lg px-2 py-1.5 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                            />
                                        </div>
                                        <div>
                                            <label htmlFor={`wizard-max-pickers-${idx}`} className="block text-xs text-text-secondary mb-1">Max Pickers</label>
                                            <input
                                                id={`wizard-max-pickers-${idx}`}
                                                type="number"
                                                min={1}
                                                max={50}
                                                value={team.max_pickers}
                                                onChange={e => updateTeam(idx, 'max_pickers', parseInt(e.target.value) || 1)}
                                                className="w-full border border-border-light rounded-lg px-2 py-1.5 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                            />
                                        </div>
                                    </div>
                                </div>
                            ))}
                            <button
                                onClick={addTeam}
                                className="flex items-center gap-1 text-sm text-emerald-600 hover:text-emerald-700 font-medium"
                            >
                                <span className="material-symbols-outlined text-base">add</span>
                                Add Team
                            </button>
                        </div>
                    )}

                    {/* Step 2: Rates */}
                    {step === 2 && (
                        <div className="space-y-4">
                            <p className="text-sm text-text-secondary">Set the default piece rate for this orchard.</p>
                            <div className="grid grid-cols-2 gap-4">
                                <div>
                                    <label className="block text-sm font-medium text-text-primary mb-1">Primary Variety</label>
                                    <select
                                        value={data.rates.variety}
                                        onChange={e => updateRates('variety', e.target.value)}
                                        aria-label="Primary variety"
                                        className="w-full border border-border-light rounded-lg px-3 py-2 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                    >
                                        {VARIETIES.map(v => <option key={v} value={v}>{v}</option>)}
                                    </select>
                                </div>
                                <div>
                                    <label htmlFor="wizard-piece-rate" className="block text-sm font-medium text-text-primary mb-1">Piece Rate ($/bucket)</label>
                                    <input
                                        id="wizard-piece-rate"
                                        type="number"
                                        min={0.1}
                                        step={0.05}
                                        value={data.rates.piece_rate}
                                        onChange={e => updateRates('piece_rate', parseFloat(e.target.value) || 0)}
                                        className="w-full border border-border-light rounded-lg px-3 py-2 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                    />
                                </div>
                            </div>
                            <div>
                                <label htmlFor="wizard-start-time" className="block text-sm font-medium text-text-primary mb-1">Default Start Time</label>
                                <input
                                    id="wizard-start-time"
                                    type="time"
                                    value={data.rates.start_time}
                                    onChange={e => updateRates('start_time', e.target.value)}
                                    className="w-40 border border-border-light rounded-lg px-3 py-2 text-sm focus:ring-2 focus:ring-emerald-500 focus:border-emerald-500"
                                />
                            </div>
                        </div>
                    )}

                    {/* Step 3: Summary */}
                    {step === 3 && (
                        <div className="space-y-4">
                            <div className="bg-emerald-50 border border-emerald-200 rounded-lg p-4">
                                <h3 className="text-sm font-semibold text-emerald-800 mb-3 flex items-center gap-2">
                                    <span className="material-symbols-outlined text-base">park</span>
                                    Orchard
                                </h3>
                                <div className="grid grid-cols-2 gap-2 text-sm">
                                    <div><span className="text-text-secondary">Code:</span> <span className="font-medium">{data.orchard.code}</span></div>
                                    <div><span className="text-text-secondary">Name:</span> <span className="font-medium">{data.orchard.name}</span></div>
                                    <div><span className="text-text-secondary">Location:</span> <span className="font-medium">{data.orchard.location || '—'}</span></div>
                                    <div><span className="text-text-secondary">Rows:</span> <span className="font-medium">{data.orchard.total_rows}</span></div>
                                </div>
                            </div>

                            <div className="bg-blue-50 border border-blue-200 rounded-lg p-4">
                                <h3 className="text-sm font-semibold text-blue-800 mb-3 flex items-center gap-2">
                                    <span className="material-symbols-outlined text-base">groups</span>
                                    Teams ({data.teams.length})
                                </h3>
                                {data.teams.map((t, i) => (
                                    <div key={i} className="text-sm text-blue-700">
                                        {t.name} — Leader: {t.leader_name || 'TBD'} — Max {t.max_pickers} pickers
                                    </div>
                                ))}
                            </div>

                            <div className="bg-amber-50 border border-amber-200 rounded-lg p-4">
                                <h3 className="text-sm font-semibold text-amber-800 mb-3 flex items-center gap-2">
                                    <span className="material-symbols-outlined text-base">payments</span>
                                    Rates
                                </h3>
                                <div className="grid grid-cols-2 gap-2 text-sm">
                                    <div><span className="text-text-secondary">Variety:</span> <span className="font-medium">{data.rates.variety}</span></div>
                                    <div><span className="text-text-secondary">Rate:</span> <span className="font-medium">${data.rates.piece_rate.toFixed(2)}/bucket</span></div>
                                    <div><span className="text-text-secondary">Start:</span> <span className="font-medium">{data.rates.start_time}</span></div>
                                </div>
                            </div>

                            {error && (
                                <div className="bg-red-50 border border-red-200 rounded-lg p-3 text-sm text-red-700 flex items-center gap-2">
                                    <span className="material-symbols-outlined text-base">error</span>
                                    {error}
                                </div>
                            )}
                        </div>
                    )}
                </div>

                {/* Footer */}
                <div className="px-6 py-4 border-t border-border-light flex items-center justify-between">
                    <div>
                        {step > 0 && (
                            <button
                                onClick={() => setStep(s => s - 1)}
                                className="flex items-center gap-1 text-sm text-text-secondary hover:text-text-primary"
                            >
                                <span className="material-symbols-outlined text-base">arrow_back</span>
                                Back
                            </button>
                        )}
                    </div>
                    <div className="flex items-center gap-3">
                        {step > 0 && step < 3 && (
                            <button
                                onClick={() => setStep(s => s + 1)}
                                className="text-sm text-text-muted hover:text-text-secondary"
                            >
                                Skip for now
                            </button>
                        )}
                        {step < 3 ? (
                            <button
                                onClick={() => setStep(s => s + 1)}
                                disabled={!canProceed()}
                                className="flex items-center gap-1 px-4 py-2 bg-emerald-600 text-white text-sm font-medium rounded-lg hover:bg-emerald-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
                            >
                                Next
                                <span className="material-symbols-outlined text-base">arrow_forward</span>
                            </button>
                        ) : (
                            <button
                                onClick={handleSubmit}
                                disabled={isSubmitting}
                                className="flex items-center gap-1 px-5 py-2 bg-emerald-600 text-white text-sm font-medium rounded-lg hover:bg-emerald-700 disabled:opacity-50 transition-colors"
                            >
                                {isSubmitting ? (
                                    <>
                                        <span className="material-symbols-outlined text-base animate-spin">progress_activity</span>
                                        Creating...
                                    </>
                                ) : (
                                    <>
                                        <span className="material-symbols-outlined text-base">check_circle</span>
                                        Create Orchard
                                    </>
                                )}
                            </button>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
}
