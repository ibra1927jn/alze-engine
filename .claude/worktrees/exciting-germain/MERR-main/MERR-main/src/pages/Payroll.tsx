/**
 * Payroll Page — Payroll Admin Department
 *
 * Refactored architecture:
 *   Payroll.tsx              — Thin orchestrator (~60 lines)
 *   usePayroll.ts            — Data hook (loading, summaries)
 *   payroll/
 *     └── PayrollTabs.tsx    — SummaryCard, PayrollDashboard, TimesheetsTab, WageCalculatorTab, ExportTab
 *   payroll/ExportHistoryTab — (existing) Export history component
 */
import React, { useState } from 'react';
import DesktopLayout, { NavItem } from '@/components/common/DesktopLayout';
import { usePayroll } from '@/hooks/usePayroll';
import { SummaryCard, PayrollDashboard, TimesheetsTab, WageCalculatorTab, ExportTab } from '@/components/views/payroll/PayrollTabs';
import ExportHistoryTab from '@/components/views/payroll/ExportHistoryTab';
import LoadingSkeleton from '@/components/ui/LoadingSkeleton';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';

const PAYROLL_NAV: NavItem[] = [
    { id: 'dashboard', label: 'Dashboard', icon: 'dashboard' },
    { id: 'timesheets', label: 'Timesheets', icon: 'schedule' },
    { id: 'wages', label: 'Wage Calculator', icon: 'calculate' },
    { id: 'export', label: 'Export', icon: 'download' },
    { id: 'history', label: 'History', icon: 'history' },
];

const Payroll: React.FC = () => {
    const [activeTab, setActiveTab] = useState('dashboard');
    const pay = usePayroll();

    const navItems = PAYROLL_NAV.map(item => ({
        ...item,
        badge: item.id === 'dashboard' && pay.compliance.workers_below_minimum > 0 ? pay.compliance.workers_below_minimum : undefined,
    }));

    if (pay.isLoading) {
        return (
            <div className="min-h-screen bg-background-light p-6">
                <div className="max-w-6xl mx-auto space-y-4">
                    <div className="grid grid-cols-2 lg:grid-cols-5 gap-3"><LoadingSkeleton type="metric" count={5} /></div>
                    <LoadingSkeleton type="table" count={8} />
                </div>
            </div>
        );
    }

    return (
        <DesktopLayout navItems={navItems} activeTab={activeTab} onTabChange={setActiveTab} title="Payroll Admin" accentColor="orange" titleIcon="payments">
            <div className="grid grid-cols-2 lg:grid-cols-5 gap-3 mb-6">
                <SummaryCard icon="inventory_2" iconColor="text-indigo-500" label="Total Buckets" value={pay.summary.total_buckets.toLocaleString()} />
                <SummaryCard icon="schedule" iconColor="text-sky-500" label="Total Hours" value={`${pay.summary.total_hours.toFixed(1)}h`} />
                <SummaryCard icon="payments" iconColor="text-emerald-500" label="Piece Rate" value={`$${pay.summary.total_piece_rate_earnings.toFixed(0)}`} />
                <SummaryCard icon="shield" iconColor="text-amber-500" label="Top-Up Required" value={`$${pay.summary.total_top_up.toFixed(0)}`} />
                <SummaryCard icon="account_balance" iconColor="text-orange-600" label="Total Payroll" value={`$${pay.summary.total_earnings.toFixed(0)}`} highlight />
            </div>

            {pay.compliance.workers_below_minimum > 0 && (
                <div className="bg-amber-50 border border-amber-200 rounded-xl px-4 py-3 flex items-center gap-3 mb-4">
                    <span className="material-symbols-outlined text-amber-600">shield</span>
                    <p className="text-sm font-medium text-amber-800">
                        Wage Shield active for {pay.compliance.workers_below_minimum} of {pay.compliance.workers_total} workers.
                        Compliance rate: {pay.compliance.compliance_rate.toFixed(0)}%
                    </p>
                </div>
            )}

            <div key={activeTab} className="animate-fade-in">
                {activeTab === 'dashboard' && <ComponentErrorBoundary componentName="Payroll Dashboard"><PayrollDashboard pickers={pay.pickers} settings={pay.settings} /></ComponentErrorBoundary>}
                {activeTab === 'timesheets' && <ComponentErrorBoundary componentName="Timesheets"><TimesheetsTab orchardId={pay.orchardId} /></ComponentErrorBoundary>}
                {activeTab === 'wages' && <ComponentErrorBoundary componentName="Wage Calculator"><WageCalculatorTab settings={pay.settings} /></ComponentErrorBoundary>}
                {activeTab === 'export' && <ComponentErrorBoundary componentName="Export"><ExportTab /></ComponentErrorBoundary>}
                {activeTab === 'history' && <ComponentErrorBoundary componentName="Export History"><ExportHistoryTab /></ComponentErrorBoundary>}
            </div>
        </DesktopLayout>
    );
};

export default Payroll;
