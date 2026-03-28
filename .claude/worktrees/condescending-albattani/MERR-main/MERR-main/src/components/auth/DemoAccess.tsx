import React from 'react';
import { Role } from '@/types';

interface DemoAccessProps {
    isSubmitting: boolean;
    onDemoAccess: (role: Role) => void;
}

const DEMO_ROLES = [
    { role: Role.MANAGER, label: 'Manager', desc: 'Command center & analytics', icon: 'admin_panel_settings', color: 'bg-indigo-50 text-indigo-700 border-indigo-200' },
    { role: Role.TEAM_LEADER, label: 'Team Leader', desc: 'Manage pickers & rows', icon: 'groups', color: 'bg-emerald-50 text-emerald-700 border-emerald-200' },
    { role: Role.RUNNER, label: 'Bucket Runner', desc: 'Logistics & scanning', icon: 'local_shipping', color: 'bg-sky-50 text-sky-700 border-sky-200' },
    { role: Role.QC_INSPECTOR, label: 'QC Inspector', desc: 'Quality & grading', icon: 'verified', color: 'bg-amber-50 text-amber-700 border-amber-200' },
    { role: Role.PAYROLL_ADMIN, label: 'Payroll Admin', desc: 'Wages & billing', icon: 'payments', color: 'bg-orange-50 text-orange-700 border-orange-200' },
    { role: Role.ADMIN, label: 'Admin', desc: 'System administration', icon: 'shield_person', color: 'bg-red-50 text-red-700 border-red-200' },
    { role: Role.HR_ADMIN, label: 'HR Admin', desc: 'Workforce & compliance', icon: 'badge', color: 'bg-purple-50 text-purple-700 border-purple-200' },
    { role: Role.LOGISTICS, label: 'Logistics', desc: 'Fleet & bin tracking', icon: 'local_shipping', color: 'bg-teal-50 text-teal-700 border-teal-200' },
] as const;

const DemoAccess: React.FC<DemoAccessProps> = ({ isSubmitting, onDemoAccess }) => (
    <div className="space-y-3">
        <p className="text-center text-text-secondary text-sm mb-5">
            Explore the platform without an account. Select a role:
        </p>

        {DEMO_ROLES.map((item) => (
            <button
                key={item.role}
                onClick={() => onDemoAccess(item.role)}
                disabled={isSubmitting}
                className={`w-full p-4 rounded-xl border flex items-center gap-4 hover:shadow-md active:scale-[0.98] transition-all disabled:opacity-50 ${item.color}`}
            >
                <div className="w-11 h-11 rounded-lg bg-white flex items-center justify-center flex-shrink-0 shadow-sm">
                    <span className="material-symbols-outlined text-xl">{item.icon}</span>
                </div>
                <div className="text-left">
                    <p className="font-bold text-base leading-tight">{item.label}</p>
                    <p className="text-sm opacity-70">{item.desc}</p>
                </div>
                <span className="material-symbols-outlined ml-auto opacity-40">arrow_forward</span>
            </button>
        ))}

        <p className="text-center text-text-muted text-xs mt-4">
            Demo mode uses local data only. For full features, create an account.
        </p>
    </div>
);

export default DemoAccess;
