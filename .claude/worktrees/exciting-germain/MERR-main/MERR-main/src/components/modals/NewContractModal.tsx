/**
 * NewContractModal.tsx — Create a new contract for an employee
 * Modal form with employee selection, dates, hourly rate, and contract type
 */
import React, { useState } from 'react';
import { Employee } from '@/services/hhrr.service';
import ModalOverlay from '@/components/ui/ModalOverlay';

interface NewContractModalProps {
    isOpen: boolean;
    onClose: () => void;
    employees: Employee[];
    onSubmit: (data: {
        employee_id: string;
        contract_type: string;
        start_date: string;
        end_date: string;
        hourly_rate: number;
    }) => void;
}

const CONTRACT_TYPES = [
    { value: 'permanent', label: 'Permanent', icon: 'all_inclusive', color: 'text-emerald-700 bg-emerald-50' },
    { value: 'seasonal', label: 'Seasonal', icon: 'wb_sunny', color: 'text-sky-700 bg-sky-50' },
    { value: 'casual', label: 'Casual', icon: 'schedule', color: 'text-text-sub bg-slate-50' },
];

const NewContractModal: React.FC<NewContractModalProps> = ({ isOpen, onClose, employees, onSubmit }) => {
    const today = new Date().toISOString().split('T')[0];
    const nextYear = new Date(Date.now() + 365 * 86400000).toISOString().split('T')[0];

    const [form, setForm] = useState({
        employee_id: '',
        contract_type: 'seasonal',
        start_date: today,
        end_date: nextYear,
        hourly_rate: 22.70,
    });
    const [errors, setErrors] = useState<Record<string, string>>({});

    if (!isOpen) return null;

    const validate = () => {
        const errs: Record<string, string> = {};
        if (!form.employee_id) errs.employee_id = 'Select an employee';
        if (!form.start_date) errs.start_date = 'Start date required';
        if (!form.end_date) errs.end_date = 'End date required';
        if (form.end_date <= form.start_date) errs.end_date = 'End must be after start';
        if (form.hourly_rate < 1) errs.hourly_rate = 'Rate must be at least $1';
        setErrors(errs);
        return Object.keys(errs).length === 0;
    };

    const handleSubmit = (e: React.FormEvent) => {
        e.preventDefault();
        if (!validate()) return;
        onSubmit(form);
        setForm({ employee_id: '', contract_type: 'seasonal', start_date: today, end_date: nextYear, hourly_rate: 22.70 });
        onClose();
    };

    return (
        <ModalOverlay onClose={onClose}>
            {/* Header */}
            <div className="flex items-center justify-between px-5 py-4 border-b border-border-light">
                <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-primary">description</span>
                    <h2 className="text-base font-bold text-text-main">New Contract</h2>
                </div>
                <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                    <span className="material-symbols-outlined">close</span>
                </button>
            </div>

            {/* Form */}
            <form onSubmit={handleSubmit} className="p-5 space-y-4">
                {/* Employee Select */}
                <div>
                    <label htmlFor="contract-employee" className="block text-xs font-bold text-text-sub mb-1">Employee</label>
                    <select
                        id="contract-employee"
                        value={form.employee_id}
                        onChange={e => setForm(prev => ({ ...prev, employee_id: e.target.value }))}
                        className={`w-full px-3 py-2.5 rounded-xl border text-sm font-medium outline-none bg-white transition-all ${errors.employee_id ? 'border-red-300' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                            }`}
                    >
                        <option value="">Select employee...</option>
                        {employees.map(emp => (
                            <option key={emp.id} value={emp.id}>{emp.full_name} — {emp.role}</option>
                        ))}
                    </select>
                    {errors.employee_id && <p className="text-xs text-danger mt-1">{errors.employee_id}</p>}
                </div>

                {/* Contract Type */}
                <div>
                    <label className="block text-xs font-bold text-text-sub mb-2">Contract Type</label>
                    <div className="grid grid-cols-3 gap-2">
                        {CONTRACT_TYPES.map(ct => (
                            <button
                                key={ct.value}
                                type="button"
                                onClick={() => setForm(prev => ({ ...prev, contract_type: ct.value }))}
                                className={`py-2.5 rounded-xl border-2 text-xs font-bold flex items-center justify-center gap-1 transition-all ${form.contract_type === ct.value
                                    ? `${ct.color} border-current ring-2 ring-current/10`
                                    : 'border-border-light text-text-muted hover:border-slate-300'
                                    }`}
                            >
                                <span className="material-symbols-outlined text-sm">{ct.icon}</span>
                                {ct.label}
                            </button>
                        ))}
                    </div>
                </div>

                {/* Dates row */}
                <div className="grid grid-cols-2 gap-3">
                    <div>
                        <label htmlFor="contract-start-date" className="block text-xs font-bold text-text-sub mb-1">Start Date</label>
                        <input
                            id="contract-start-date"
                            type="date"
                            value={form.start_date}
                            onChange={e => setForm(prev => ({ ...prev, start_date: e.target.value }))}
                            className={`w-full px-3 py-2.5 rounded-xl border text-sm font-medium outline-none transition-all ${errors.start_date ? 'border-red-300' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                                }`}
                        />
                        {errors.start_date && <p className="text-xs text-danger mt-1">{errors.start_date}</p>}
                    </div>
                    <div>
                        <label htmlFor="contract-end-date" className="block text-xs font-bold text-text-sub mb-1">End Date</label>
                        <input
                            id="contract-end-date"
                            type="date"
                            value={form.end_date}
                            onChange={e => setForm(prev => ({ ...prev, end_date: e.target.value }))}
                            className={`w-full px-3 py-2.5 rounded-xl border text-sm font-medium outline-none transition-all ${errors.end_date ? 'border-red-300' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                                }`}
                        />
                        {errors.end_date && <p className="text-xs text-danger mt-1">{errors.end_date}</p>}
                    </div>
                </div>

                {/* Hourly Rate */}
                <div>
                    <label htmlFor="contract-hourly-rate" className="block text-xs font-bold text-text-sub mb-1">Hourly Rate (NZD)</label>
                    <div className="relative">
                        <span className="absolute left-3 top-1/2 -translate-y-1/2 text-text-muted text-sm font-bold">$</span>
                        <input
                            id="contract-hourly-rate"
                            type="number"
                            step="0.10"
                            min={1}
                            value={form.hourly_rate}
                            onChange={e => setForm(prev => ({ ...prev, hourly_rate: parseFloat(e.target.value) || 0 }))}
                            className={`w-full pl-7 pr-3 py-2.5 rounded-xl border text-sm font-medium outline-none transition-all ${errors.hourly_rate ? 'border-red-300' : 'border-border-light focus:border-primary focus:ring-2 focus:ring-primary/10'
                                }`}
                        />
                    </div>
                    {errors.hourly_rate && <p className="text-xs text-danger mt-1">{errors.hourly_rate}</p>}
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
                        <span className="material-symbols-outlined text-sm">add</span>
                        Create Contract
                    </button>
                </div>
            </form>
        </ModalOverlay>
    );
};

export default NewContractModal;
