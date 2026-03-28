/**
 * SeasonalPlanningTab — HR Workforce Planning Dashboard
 * Contract expiry forecasting + workforce gap analysis
 */
import React, { useState, useMemo } from 'react';
import { Employee } from '@/services/hhrr.service';
import FilterBar from '@/components/ui/FilterBar';

interface SeasonalPlanningTabProps {
    employees: Employee[];
}

const SeasonalPlanningTab: React.FC<SeasonalPlanningTabProps> = ({ employees }) => {
    const [searchQuery, setSearchQuery] = useState('');
    const [activeFilters, setActiveFilters] = useState<Record<string, string>>({});

    const contractTypes = useMemo(() =>
        [...new Set(employees.map(e => e.contract_type))].sort(),
        [employees]
    );

    const today = new Date();
    const in30 = new Date(today.getTime() + 30 * 86400000);
    const in60 = new Date(today.getTime() + 60 * 86400000);
    const in90 = new Date(today.getTime() + 90 * 86400000);

    const activeEmployees = useMemo(() => employees.filter(e => {
        const matchActive = e.status === 'active';
        const matchSearch = !searchQuery ||
            e.full_name.toLowerCase().includes(searchQuery.toLowerCase());
        const matchType = !activeFilters.contract_type || e.contract_type === activeFilters.contract_type;
        return matchActive && matchSearch && matchType;
    }), [employees, searchQuery, activeFilters]);

    // Contract expiry forecasts
    const expiring30 = activeEmployees.filter(e => {
        if (!e.contract_end) return false;
        const end = new Date(e.contract_end);
        return end >= today && end <= in30;
    });

    const expiring60 = activeEmployees.filter(e => {
        if (!e.contract_end) return false;
        const end = new Date(e.contract_end);
        return end > in30 && end <= in60;
    });

    const expiring90 = activeEmployees.filter(e => {
        if (!e.contract_end) return false;
        const end = new Date(e.contract_end);
        return end > in60 && end <= in90;
    });

    const permanentCount = activeEmployees.filter(e => e.contract_type === 'permanent').length;
    const seasonalCount = activeEmployees.filter(e => e.contract_type === 'seasonal').length;
    const casualCount = activeEmployees.filter(e => e.contract_type === 'casual').length;

    // Visa expiry alerts
    const visaExpiring = activeEmployees.filter(e => {
        if (!e.visa_expiry || e.visa_status === 'citizen' || e.visa_status === 'resident') return false;
        const exp = new Date(e.visa_expiry);
        return exp >= today && exp <= in90;
    });

    const daysUntil = (dateStr: string) => {
        const days = Math.ceil((new Date(dateStr).getTime() - today.getTime()) / 86400000);
        return days;
    };

    const urgencyColor = (days: number) => {
        if (days <= 14) return 'text-red-600 bg-red-50 border-red-200';
        if (days <= 30) return 'text-amber-600 bg-amber-50 border-amber-200';
        return 'text-blue-600 bg-blue-50 border-blue-200';
    };

    return (
        <div className="space-y-5">
            {/* Filter Bar */}
            <FilterBar
                searchValue={searchQuery}
                onSearchChange={setSearchQuery}
                searchPlaceholder="Search employees..."
                filters={[
                    { key: 'contract_type', label: 'Contract Type', options: contractTypes, icon: 'description' },
                ]}
                activeFilters={activeFilters}
                onFilterChange={(key, value) => setActiveFilters(prev => ({ ...prev, [key]: value }))}
                onClearAll={() => { setSearchQuery(''); setActiveFilters({}); }}
            />

            {/* Workforce Summary */}
            <div className="grid grid-cols-2 lg:grid-cols-4 gap-3">
                <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light text-center">
                    <p className="text-2xl font-black text-text-primary">{activeEmployees.length}</p>
                    <p className="text-xs text-text-secondary font-medium mt-1">Active Headcount</p>
                </div>
                <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light text-center">
                    <p className="text-2xl font-black text-emerald-600">{permanentCount}</p>
                    <p className="text-xs text-text-secondary font-medium mt-1">Permanent</p>
                </div>
                <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light text-center">
                    <p className="text-2xl font-black text-amber-600">{seasonalCount}</p>
                    <p className="text-xs text-text-secondary font-medium mt-1">Seasonal</p>
                </div>
                <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light text-center">
                    <p className="text-2xl font-black text-blue-600">{casualCount}</p>
                    <p className="text-xs text-text-secondary font-medium mt-1">Casual</p>
                </div>
            </div>

            {/* Contract Expiry Forecast */}
            <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
                <h3 className="font-bold text-text-primary mb-1 flex items-center gap-2">
                    <span className="material-symbols-outlined text-amber-500">event_upcoming</span>
                    Contract Expiry Forecast
                </h3>
                <p className="text-xs text-text-secondary mb-4">Contracts expiring in the next 90 days</p>

                <div className="grid grid-cols-3 gap-3 mb-4">
                    <div className="rounded-lg bg-red-50 border border-red-200 p-3 text-center">
                        <p className="text-xl font-black text-red-600">{expiring30.length}</p>
                        <p className="text-xs text-red-500 font-medium">Next 30 days</p>
                    </div>
                    <div className="rounded-lg bg-amber-50 border border-amber-200 p-3 text-center">
                        <p className="text-xl font-black text-amber-600">{expiring60.length}</p>
                        <p className="text-xs text-amber-500 font-medium">31-60 days</p>
                    </div>
                    <div className="rounded-lg bg-blue-50 border border-blue-200 p-3 text-center">
                        <p className="text-xl font-black text-blue-600">{expiring90.length}</p>
                        <p className="text-xs text-blue-500 font-medium">61-90 days</p>
                    </div>
                </div>

                {/* Timeline List */}
                {[...expiring30, ...expiring60, ...expiring90].length === 0 ? (
                    <p className="text-center text-text-muted py-3 text-sm">No contracts expiring in the next 90 days ✅</p>
                ) : (
                    <div className="space-y-2 max-h-64 overflow-y-auto">
                        {[...expiring30, ...expiring60, ...expiring90]
                            .sort((a, b) => new Date(a.contract_end!).getTime() - new Date(b.contract_end!).getTime())
                            .map(emp => {
                                const days = daysUntil(emp.contract_end!);
                                return (
                                    <div key={emp.id} className={`flex items-center gap-3 p-3 rounded-lg border ${urgencyColor(days)}`}>
                                        <div className="flex-1">
                                            <p className="text-sm font-semibold">{emp.full_name}</p>
                                            <p className="text-xs opacity-70">{emp.role} • {emp.contract_type}</p>
                                        </div>
                                        <div className="text-right">
                                            <p className="text-sm font-bold">{days}d</p>
                                            <p className="text-[10px] opacity-70">{new Date(emp.contract_end!).toLocaleDateString('en-NZ')}</p>
                                        </div>
                                    </div>
                                );
                            })}
                    </div>
                )}
            </div>

            {/* Visa Expiry Alerts */}
            <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
                <h3 className="font-bold text-text-primary mb-1 flex items-center gap-2">
                    <span className="material-symbols-outlined text-red-500">flight</span>
                    Visa Expiry Alerts
                </h3>
                <p className="text-xs text-text-secondary mb-4">Work visas expiring in the next 90 days</p>
                {visaExpiring.length === 0 ? (
                    <p className="text-center text-text-muted py-3 text-sm">No visa expiries in the next 90 days ✅</p>
                ) : (
                    <div className="space-y-2">
                        {visaExpiring
                            .sort((a, b) => new Date(a.visa_expiry!).getTime() - new Date(b.visa_expiry!).getTime())
                            .map(emp => {
                                const days = daysUntil(emp.visa_expiry!);
                                return (
                                    <div key={emp.id} className={`flex items-center gap-3 p-3 rounded-lg border ${urgencyColor(days)}`}>
                                        <span className="material-symbols-outlined text-lg">badge</span>
                                        <div className="flex-1">
                                            <p className="text-sm font-semibold">{emp.full_name}</p>
                                            <p className="text-xs opacity-70">{emp.visa_status}</p>
                                        </div>
                                        <div className="text-right">
                                            <p className="text-sm font-bold">{days}d</p>
                                            <p className="text-[10px] opacity-70">{new Date(emp.visa_expiry!).toLocaleDateString('en-NZ')}</p>
                                        </div>
                                    </div>
                                );
                            })}
                    </div>
                )}
            </div>

            {/* Projected Headcount */}
            <div className="bg-white rounded-xl p-5 shadow-sm border border-border-light">
                <h3 className="font-bold text-text-primary mb-1 flex items-center gap-2">
                    <span className="material-symbols-outlined text-indigo-500">trending_down</span>
                    Projected Headcount
                </h3>
                <p className="text-xs text-text-secondary mb-4">If no contracts are renewed</p>
                <div className="grid grid-cols-4 gap-3">
                    {[
                        { label: 'Now', count: activeEmployees.length, color: 'bg-emerald-500' },
                        { label: '+30d', count: activeEmployees.length - expiring30.length, color: 'bg-amber-500' },
                        { label: '+60d', count: activeEmployees.length - expiring30.length - expiring60.length, color: 'bg-orange-500' },
                        { label: '+90d', count: activeEmployees.length - expiring30.length - expiring60.length - expiring90.length, color: 'bg-red-500' },
                    ].map(item => (
                        <div key={item.label} className="text-center">
                            <div className="h-20 flex items-end justify-center mb-2">
                                <div className={`w-10 ${item.color} rounded-t-lg transition-all`} style={{ height: `${(item.count / (activeEmployees.length || 1)) * 100}%`, minHeight: '8px' }} />
                            </div>
                            <p className="text-lg font-black text-text-primary">{item.count}</p>
                            <p className="text-xs text-text-secondary">{item.label}</p>
                        </div>
                    ))}
                </div>
            </div>
        </div>
    );
};

export default SeasonalPlanningTab;
