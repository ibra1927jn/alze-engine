/**
 * ContractsTab.tsx — HR Contract Management
 * Contract cards with Renew / Terminate action buttons
 */
import React, { useState, useMemo } from 'react';
import { Employee, HRSummary } from '@/services/hhrr.service';
import { updateContract } from '@/services/hhrr.service';
import FilterBar from '@/components/ui/FilterBar';
import InlineEdit from '@/components/ui/InlineEdit';

interface ContractsTabProps {
    employees: Employee[];
    summary: HRSummary;
    onRefresh?: () => void;
}

const ContractsTab: React.FC<ContractsTabProps> = ({ employees, summary, onRefresh }) => {
    const [confirmId, setConfirmId] = useState<string | null>(null);
    const [toast, setToast] = useState<{ message: string; type: 'success' | 'error' } | null>(null);
    const [searchQuery, setSearchQuery] = useState('');
    const [activeFilters, setActiveFilters] = useState<Record<string, string>>({});

    const filterOptions = useMemo(() => ({
        types: [...new Set(employees.map(e => e.contract_type))].sort(),
        statuses: [...new Set(employees.map(e => e.status))].sort(),
    }), [employees]);

    const filteredEmployees = useMemo(() => employees.filter(emp => {
        const matchesSearch = !searchQuery ||
            emp.full_name.toLowerCase().includes(searchQuery.toLowerCase());
        const matchesType = !activeFilters.contract_type || emp.contract_type === activeFilters.contract_type;
        const matchesStatus = !activeFilters.status || emp.status === activeFilters.status;
        return matchesSearch && matchesType && matchesStatus;
    }), [employees, searchQuery, activeFilters]);

    const showToast = (message: string, type: 'success' | 'error' = 'success') => {
        setToast({ message, type });
        setTimeout(() => setToast(null), 3000);
    };

    const handleRenew = (empId: string, empName: string) => {
        const nextYear = new Date();
        nextYear.setFullYear(nextYear.getFullYear() + 1);
        updateContract(empId, {
            status: 'active',
            end_date: nextYear.toISOString().split('T')[0],
        });
        showToast(`Contract renewed for ${empName} until ${nextYear.toLocaleDateString('en-NZ')}`);
        onRefresh?.();
    };

    const handleTerminate = (empId: string, empName: string) => {
        updateContract(empId, { status: 'terminated' });
        showToast(`Contract terminated for ${empName}`, 'error');
        setConfirmId(null);
        onRefresh?.();
    };

    return (
        <div className="space-y-4 relative">
            {/* Toast */}
            {toast && (
                <div className={`fixed top-4 right-4 z-50 px-4 py-3 rounded-xl shadow-lg text-sm font-medium flex items-center gap-2
                    ${toast.type === 'success' ? 'bg-emerald-600 text-white' : 'bg-red-600 text-white'}`}>
                    <span className="material-symbols-outlined text-sm">
                        {toast.type === 'success' ? 'check_circle' : 'warning'}
                    </span>
                    {toast.message}
                </div>
            )}

            {/* Warning Banner */}
            {summary.pendingContracts > 0 && (
                <div className="bg-amber-50 border border-amber-200 rounded-xl p-4 flex items-center gap-3">
                    <span className="material-symbols-outlined text-amber-600">info</span>
                    <p className="text-sm text-amber-800 font-medium">{summary.pendingContracts} contracts need renewal within 30 days</p>
                </div>
            )}
            {/* Filter Bar */}
            <FilterBar
                searchValue={searchQuery}
                onSearchChange={setSearchQuery}
                searchPlaceholder="Search contracts..."
                filters={[
                    { key: 'contract_type', label: 'Type', options: filterOptions.types, icon: 'description' },
                    { key: 'status', label: 'Status', options: filterOptions.statuses, icon: 'toggle_on' },
                ]}
                activeFilters={activeFilters}
                onFilterChange={(key, value) => setActiveFilters(prev => ({ ...prev, [key]: value }))}
                onClearAll={() => { setSearchQuery(''); setActiveFilters({}); }}
            />

            {/* Contract Cards */}
            <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
                {filteredEmployees.map(emp => (
                    <div key={emp.id} className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                        <div className="flex items-center justify-between mb-2">
                            <h3 className="font-bold text-text-primary text-sm">{emp.full_name}</h3>
                            <span className={`text-[10px] font-bold uppercase px-2 py-0.5 rounded-full ${emp.contract_type === 'permanent' ? 'bg-emerald-50 text-emerald-700' :
                                emp.contract_type === 'seasonal' ? 'bg-sky-50 text-sky-700' :
                                    'bg-surface-secondary text-text-secondary'
                                }`}>{emp.contract_type}</span>
                        </div>
                        <div className="flex items-center gap-4 text-xs text-text-secondary mb-3">
                            <span className="flex items-center gap-1">
                                <span className="material-symbols-outlined text-xs">event</span>
                                {new Date(emp.contract_start).toLocaleDateString('en-NZ', { month: 'short', year: 'numeric' })}
                                {emp.contract_end && (
                                    <>
                                        <span className="text-text-disabled mx-0.5">→</span>
                                        <InlineEdit
                                            value={emp.contract_end}
                                            onSave={(val) => updateContract(emp.id, { end_date: val })}
                                            type="date"
                                        />
                                    </>
                                )}
                            </span>
                            <span className="flex items-center gap-1">
                                <span className="material-symbols-outlined text-xs">attach_money</span>
                                <InlineEdit
                                    value={String(emp.hourly_rate)}
                                    onSave={(val) => updateContract(emp.id, { hourly_rate: parseFloat(val) || emp.hourly_rate })}
                                    type="number"
                                    prefix="$"
                                    suffix="/hr"
                                    minWidth="50px"
                                />
                            </span>
                        </div>

                        {/* Action Buttons */}
                        <div className="flex gap-2">
                            <button
                                onClick={() => handleRenew(emp.id, emp.full_name)}
                                className="flex-1 py-1.5 rounded-lg bg-emerald-50 text-emerald-700 text-xs font-bold hover:bg-emerald-100 transition-colors flex items-center justify-center gap-1"
                            >
                                <span className="material-symbols-outlined text-xs">autorenew</span>
                                Renew
                            </button>
                            {confirmId === emp.id ? (
                                <div className="flex-1 flex gap-1">
                                    <button
                                        onClick={() => handleTerminate(emp.id, emp.full_name)}
                                        className="flex-1 py-1.5 rounded-lg bg-red-600 text-white text-xs font-bold hover:bg-red-700 transition-colors"
                                    >
                                        Confirm
                                    </button>
                                    <button
                                        onClick={() => setConfirmId(null)}
                                        className="flex-1 py-1.5 rounded-lg bg-surface-secondary text-text-secondary text-xs font-bold hover:bg-surface-secondary transition-colors"
                                    >
                                        Cancel
                                    </button>
                                </div>
                            ) : (
                                <button
                                    onClick={() => setConfirmId(emp.id)}
                                    className="flex-1 py-1.5 rounded-lg bg-red-50 text-red-700 text-xs font-bold hover:bg-red-100 transition-colors flex items-center justify-center gap-1"
                                >
                                    <span className="material-symbols-outlined text-xs">cancel</span>
                                    Terminate
                                </button>
                            )}
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
};

export default ContractsTab;
