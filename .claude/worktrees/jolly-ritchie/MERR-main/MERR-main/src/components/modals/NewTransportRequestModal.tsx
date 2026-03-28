/**
 * NewTransportRequestModal.tsx — Create a new transport request
 * Modal form for requesting bin pickup from a zone
 */
import React, { useState } from 'react';
import ModalOverlay from '@/components/ui/ModalOverlay';

interface NewTransportRequestModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSubmit: (data: {
        zone: string;
        bins_count: number;
        priority: 'normal' | 'high' | 'urgent';
        notes: string;
    }) => void;
}

const ZONES = ['A1', 'A2', 'A3', 'A4', 'B1', 'B2', 'B3', 'C1'];
const PRIORITIES: { value: 'normal' | 'high' | 'urgent'; label: string; icon: string; color: string }[] = [
    { value: 'normal', label: 'Normal', icon: 'schedule', color: 'border-slate-200 text-text-sub bg-slate-50' },
    { value: 'high', label: 'High', icon: 'arrow_upward', color: 'border-amber-200 text-amber-700 bg-amber-50' },
    { value: 'urgent', label: 'Urgent', icon: 'priority_high', color: 'border-red-200 text-red-700 bg-red-50' },
];

const NewTransportRequestModal: React.FC<NewTransportRequestModalProps> = ({ isOpen, onClose, onSubmit }) => {
    const [form, setForm] = useState({
        zone: 'A1',
        bins_count: 10,
        priority: 'normal' as 'normal' | 'high' | 'urgent',
        notes: '',
    });
    const [errors, setErrors] = useState<Record<string, string>>({});

    if (!isOpen) return null;

    const validate = () => {
        const errs: Record<string, string> = {};
        if (form.bins_count < 1) errs.bins_count = 'At least 1 bin required';
        setErrors(errs);
        return Object.keys(errs).length === 0;
    };

    const handleSubmit = (e: React.FormEvent) => {
        e.preventDefault();
        if (!validate()) return;
        onSubmit(form);
        setForm({ zone: 'A1', bins_count: 10, priority: 'normal', notes: '' });
        onClose();
    };

    return (
        <ModalOverlay onClose={onClose}>
            {/* Header */}
            <div className="flex items-center justify-between px-5 py-4 border-b border-border-light">
                <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-primary">local_shipping</span>
                    <h2 className="text-base font-bold text-text-main">New Transport Request</h2>
                </div>
                <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                    <span className="material-symbols-outlined">close</span>
                </button>
            </div>

            {/* Form */}
            <form onSubmit={handleSubmit} className="p-5 space-y-4">
                {/* Zone + Bins row */}
                <div className="grid grid-cols-2 gap-3">
                    <div>
                        <label htmlFor="transport-zone" className="block text-xs font-bold text-text-sub mb-1">Pickup Zone</label>
                        <select
                            id="transport-zone"
                            value={form.zone}
                            onChange={e => setForm(prev => ({ ...prev, zone: e.target.value }))}
                            className="w-full px-3 py-2.5 rounded-xl border border-border-light text-sm font-medium outline-none focus:border-primary focus:ring-2 focus:ring-primary/10 bg-white transition-all"
                        >
                            {ZONES.map(z => <option key={z} value={z}>{z}</option>)}
                        </select>
                    </div>
                    <div>
                        <label htmlFor="transport-bins" className="block text-xs font-bold text-text-sub mb-1">Bins to Collect</label>
                        <input
                            id="transport-bins"
                            type="number"
                            min={1}
                            value={form.bins_count}
                            onChange={e => setForm(prev => ({ ...prev, bins_count: parseInt(e.target.value) || 0 }))}
                            className={`w-full px-3 py-2.5 rounded-xl border text-sm font-medium outline-none transition-all ${errors.bins_count ? 'border-red-300' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                                }`}
                        />
                        {errors.bins_count && <p className="text-xs text-danger mt-1">{errors.bins_count}</p>}
                    </div>
                </div>

                {/* Priority Selection */}
                <div>
                    <label className="block text-xs font-bold text-text-sub mb-2">Priority</label>
                    <div className="grid grid-cols-3 gap-2">
                        {PRIORITIES.map(p => (
                            <button
                                key={p.value}
                                type="button"
                                onClick={() => setForm(prev => ({ ...prev, priority: p.value }))}
                                className={`py-2.5 rounded-xl border-2 text-xs font-bold flex items-center justify-center gap-1 transition-all ${form.priority === p.value
                                    ? `${p.color} border-current ring-2 ring-current/10`
                                    : 'border-border-light text-text-muted hover:border-slate-300'
                                    }`}
                            >
                                <span className="material-symbols-outlined text-sm">{p.icon}</span>
                                {p.label}
                            </button>
                        ))}
                    </div>
                </div>

                {/* Notes */}
                <div>
                    <label className="block text-xs font-bold text-text-sub mb-1">Notes (optional)</label>
                    <textarea
                        placeholder="Any special instructions..."
                        value={form.notes}
                        onChange={e => setForm(prev => ({ ...prev, notes: e.target.value }))}
                        rows={2}
                        className="w-full px-3 py-2.5 rounded-xl border border-border-light text-sm font-medium outline-none focus:border-primary focus:ring-2 focus:ring-primary/10 resize-none transition-all"
                    />
                </div>

                {/* Actions */}
                <div className="flex gap-2 pt-2">
                    <button
                        type="button"
                        onClick={onClose}
                        className="flex-1 py-2.5 rounded-xl border border-border-light text-sm font-bold text-text-sub hover:bg-slate-50 transition-colors"
                    >
                        Cancel
                    </button>
                    <button
                        type="submit"
                        className="flex-1 py-2.5 rounded-xl gradient-primary glow-primary text-white text-sm font-bold transition-all flex items-center justify-center gap-1.5 active:scale-95"
                    >
                        <span className="material-symbols-outlined text-sm">send</span>
                        Submit Request
                    </button>
                </div>
            </form>
        </ModalOverlay>
    );
};

export default NewTransportRequestModal;
