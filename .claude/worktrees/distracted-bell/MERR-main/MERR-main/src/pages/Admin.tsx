/**
 * Admin.tsx — System Admin Dashboard
 *
 * Standardized: uses DesktopLayout (back-office role) + ComponentErrorBoundary
 * Multi-orchard overview, user management, and compliance monitoring.
 */
import React, { useState, useEffect, useCallback } from 'react';
import DesktopLayout, { NavItem } from '@/components/common/DesktopLayout';
import EmptyState from '@/components/ui/EmptyState';
import { adminService, OrchardOverview, UserRecord } from '@/services/admin.service';
import { AuditLogViewer } from '@/components/AuditLogViewer';
import LoadingSkeleton from '@/components/ui/LoadingSkeleton';
import SetupWizard from '@/components/common/SetupWizard';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';

type AdminTab = 'orchards' | 'users' | 'compliance' | 'audit';

const ADMIN_NAV_ITEMS: NavItem[] = [
    { id: 'orchards', label: 'Orchards', icon: 'apartment' },
    { id: 'users', label: 'Users', icon: 'group' },
    { id: 'compliance', label: 'Compliance', icon: 'monitoring' },
    { id: 'audit', label: 'Audit Log', icon: 'shield' },
];

const ROLE_LABELS: Record<string, { label: string; color: string }> = {
    manager: { label: 'Manager', color: 'bg-purple-100 text-purple-700' },
    team_leader: { label: 'Team Leader', color: 'bg-blue-100 text-blue-700' },
    runner: { label: 'Runner', color: 'bg-green-100 text-green-700' },
    qc_inspector: { label: 'QC Inspector', color: 'bg-amber-100 text-amber-700' },
    payroll_admin: { label: 'Payroll Admin', color: 'bg-indigo-100 text-indigo-700' },
    admin: { label: 'Admin', color: 'bg-red-100 text-red-700' },
};

export default function Admin() {
    const [activeTab, setActiveTab] = useState<AdminTab>('orchards');
    const [orchards, setOrchards] = useState<OrchardOverview[]>([]);
    const [users, setUsers] = useState<UserRecord[]>([]);
    const [userSearch, setUserSearch] = useState('');
    const [roleFilter, setRoleFilter] = useState('');
    const [isLoading, setIsLoading] = useState(true);
    const [showWizard, setShowWizard] = useState(false);

    const loadData = useCallback(async () => {
        setIsLoading(true);
        const [orchardData, userData] = await Promise.all([
            adminService.getAllOrchards(),
            adminService.getAllUsers({ role: roleFilter || undefined, search: userSearch || undefined }),
        ]);
        setOrchards(orchardData);
        setUsers(userData);
        setIsLoading(false);
    }, [roleFilter, userSearch]);

    useEffect(() => {
        loadData();
    }, [loadData]);

    const handleRoleChange = async (userId: string, newRole: string) => {
        const success = await adminService.updateUserRole(userId, newRole);
        if (success) loadData();
    };

    const handleToggleActive = async (user: UserRecord) => {
        const success = user.is_active
            ? await adminService.deactivateUser(user.id)
            : await adminService.reactivateUser(user.id);
        if (success) loadData();
    };

    const totalUsers = users.length;
    const activeUsers = users.filter(u => u.is_active).length;

    const navItems = ADMIN_NAV_ITEMS.map(item => ({
        ...item,
        badge: item.id === 'users' ? activeUsers : undefined,
    }));

    if (isLoading) {
        return (
            <div className="min-h-screen bg-background-light p-6">
                <div className="max-w-6xl mx-auto space-y-4">
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                        <LoadingSkeleton type="card" count={4} />
                    </div>
                </div>
            </div>
        );
    }

    const renderContent = () => {
        switch (activeTab) {
            case 'orchards':
                return (
                    <ComponentErrorBoundary componentName="Orchards">
                        <div className="space-y-4">
                            <div className="flex items-center justify-between">
                                <h2 className="text-lg font-semibold text-text-primary">All Orchards</h2>
                                <button
                                    onClick={() => setShowWizard(true)}
                                    className="flex items-center gap-1 bg-emerald-600 text-white px-3 py-1.5 rounded-lg text-xs font-medium hover:bg-emerald-700 transition-colors"
                                >
                                    <span className="material-symbols-outlined text-sm">add</span>
                                    New Orchard
                                </button>
                            </div>
                            {orchards.length > 0 ? (
                                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                                    {orchards.map(orch => (
                                        <div key={orch.id} className="bg-white rounded-lg border border-border-light shadow-sm p-5">
                                            <div className="flex items-start justify-between mb-4">
                                                <div>
                                                    <h3 className="text-base font-semibold text-text-primary">{orch.name}</h3>
                                                    <p className="text-xs text-text-secondary mt-0.5">{orch.total_rows} rows</p>
                                                </div>
                                                <span className="bg-green-100 text-green-700 text-xs font-medium px-2 py-0.5 rounded-full">
                                                    Active
                                                </span>
                                            </div>
                                            <div className="grid grid-cols-3 gap-3 text-center">
                                                <div>
                                                    <div className="text-xl font-bold text-text-primary">{orch.active_pickers}</div>
                                                    <div className="text-xs text-text-secondary">Pickers</div>
                                                </div>
                                                <div>
                                                    <div className="text-xl font-bold text-text-primary">{orch.today_buckets}</div>
                                                    <div className="text-xs text-text-secondary">Buckets</div>
                                                </div>
                                                <div>
                                                    <div className="text-xl font-bold text-green-600">{orch.compliance_score}%</div>
                                                    <div className="text-xs text-text-secondary">Compliance</div>
                                                </div>
                                            </div>
                                        </div>
                                    ))}
                                </div>
                            ) : (
                                <EmptyState icon="apartment" title="No orchards found" compact />
                            )}
                        </div>
                    </ComponentErrorBoundary>
                );

            case 'users':
                return (
                    <ComponentErrorBoundary componentName="Users">
                        <div className="space-y-4">
                            <div className="flex flex-col md:flex-row gap-3">
                                <div className="relative flex-1">
                                    <span className="material-symbols-outlined text-base text-text-muted absolute left-3 top-1/2 -translate-y-1/2">search</span>
                                    <input
                                        type="text"
                                        placeholder="Search users..."
                                        value={userSearch}
                                        onChange={(e) => setUserSearch(e.target.value)}
                                        aria-label="Search users"
                                        className="w-full pl-9 pr-3 py-2.5 border border-border-light rounded-lg text-sm focus:ring-2 focus:ring-indigo-500 focus:border-indigo-500"
                                    />
                                </div>
                                <div className="relative">
                                    <select
                                        value={roleFilter}
                                        onChange={(e) => setRoleFilter(e.target.value)}
                                        title="Filter by role"
                                        aria-label="Filter by role"
                                        className="appearance-none bg-white border border-border-light rounded-lg px-3 py-2.5 pr-8 text-sm focus:ring-2 focus:ring-indigo-500"
                                    >
                                        <option value="">All Roles</option>
                                        <option value="manager">Managers</option>
                                        <option value="team_leader">Team Leaders</option>
                                        <option value="runner">Runners</option>
                                        <option value="qc_inspector">QC Inspectors</option>
                                        <option value="admin">Admins</option>
                                    </select>
                                    <span className="material-symbols-outlined text-sm text-text-muted absolute right-2.5 top-1/2 -translate-y-1/2 pointer-events-none">expand_more</span>
                                </div>
                            </div>

                            <div className="bg-white rounded-lg border border-border-light shadow-sm overflow-hidden">
                                <div className="divide-y divide-border-light">
                                    {users.map(user => {
                                        const roleInfo = ROLE_LABELS[user.role] || { label: user.role, color: 'bg-surface-secondary text-text-primary' };
                                        return (
                                            <div key={user.id} className="px-4 py-3 flex items-center gap-3">
                                                <div className={`w-8 h-8 rounded-full flex items-center justify-center text-xs font-medium ${user.is_active ? 'bg-surface-secondary text-text-secondary' : 'bg-red-50 text-red-400'}`}>
                                                    {user.full_name?.split(' ').map(n => n[0]).join('') || '?'}
                                                </div>
                                                <div className="flex-1 min-w-0">
                                                    <p className={`text-sm font-medium truncate ${user.is_active ? 'text-text-primary' : 'text-text-muted line-through'}`}>
                                                        {user.full_name || 'Unnamed'}
                                                    </p>
                                                    <p className="text-xs text-text-secondary truncate">{user.email}</p>
                                                </div>
                                                <span className={`px-2 py-0.5 rounded-full text-xs font-medium ${roleInfo.color}`}>
                                                    {roleInfo.label}
                                                </span>
                                                <div className="flex items-center gap-1">
                                                    <button
                                                        onClick={() => handleToggleActive(user)}
                                                        title={user.is_active ? 'Deactivate user' : 'Reactivate user'}
                                                        className={`p-1.5 rounded-md transition-colors ${user.is_active
                                                            ? 'text-text-muted hover:text-red-500 hover:bg-red-50'
                                                            : 'text-green-400 hover:text-green-600 hover:bg-green-50'
                                                            }`}
                                                    >
                                                        {user.is_active ? <span className="material-symbols-outlined text-base">person_off</span> : <span className="material-symbols-outlined text-base">person_add</span>}
                                                    </button>
                                                    <select
                                                        value={user.role}
                                                        onChange={(e) => handleRoleChange(user.id, e.target.value)}
                                                        title={`Change role for ${user.full_name}`}
                                                        aria-label={`Change role for ${user.full_name}`}
                                                        className="text-xs border border-border-light rounded-md px-1.5 py-1 bg-background-light"
                                                    >
                                                        <option value="manager">Manager</option>
                                                        <option value="team_leader">Team Leader</option>
                                                        <option value="runner">Runner</option>
                                                        <option value="qc_inspector">QC Inspector</option>
                                                        <option value="payroll_admin">Payroll Admin</option>
                                                        <option value="admin">Admin</option>
                                                    </select>
                                                </div>
                                            </div>
                                        );
                                    })}
                                    {users.length === 0 && (
                                        <EmptyState icon="group" title="No users found" compact />
                                    )}
                                </div>
                            </div>
                        </div>
                    </ComponentErrorBoundary>
                );

            case 'compliance':
                return (
                    <ComponentErrorBoundary componentName="Compliance">
                        <div className="space-y-4">
                            <h2 className="text-lg font-semibold text-text-primary">Compliance Overview</h2>
                            <div className="bg-white rounded-lg border border-border-light shadow-sm p-5">
                                <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
                                    <ComplianceCard title="NZ Employment Act" score={98} status="Compliant" details="All wages above minimum threshold" />
                                    <ComplianceCard title="Safety Verification" score={95} status="2 pending" details="95 of 97 workers verified" />
                                    <ComplianceCard title="Audit Trail" score={100} status="Active" details="All actions logged" />
                                </div>
                            </div>
                            <div className="bg-white rounded-lg border border-border-light shadow-sm p-5">
                                <h3 className="text-sm font-semibold text-text-primary mb-3">Cross-Orchard Status</h3>
                                {orchards.map(orch => (
                                    <div key={orch.id} className="flex items-center justify-between py-2 border-b border-border-light last:border-0">
                                        <span className="text-sm font-medium text-text-primary">{orch.name}</span>
                                        <div className="flex items-center gap-2">
                                            <div className="w-24 h-2 bg-surface-secondary rounded-full overflow-hidden">
                                                <div className="h-full bg-green-500 rounded-full" style={{ width: `${orch.compliance_score}%` }} />
                                            </div>
                                            <span className="text-xs text-text-secondary w-10 text-right">{orch.compliance_score}%</span>
                                        </div>
                                    </div>
                                ))}
                            </div>
                        </div>
                    </ComponentErrorBoundary>
                );

            case 'audit':
                return (
                    <ComponentErrorBoundary componentName="Audit Log">
                        <AuditLogViewer />
                    </ComponentErrorBoundary>
                );

            default:
                return (
                    <ComponentErrorBoundary componentName="Orchards">
                        <EmptyState icon="apartment" title="Select a tab" compact />
                    </ComponentErrorBoundary>
                );
        }
    };

    return (
        <>
            <DesktopLayout
                navItems={navItems}
                activeTab={activeTab}
                onTabChange={(id) => setActiveTab(id as AdminTab)}
                title="HarvestPro Admin"
                accentColor="red"
                titleIcon="shield"
            >
                {/* Summary Cards */}
                <div className="grid grid-cols-2 lg:grid-cols-4 gap-3 mb-6">
                    <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                        <div className="flex items-center gap-2 mb-1.5">
                            <span className="material-symbols-outlined text-emerald-500 text-lg">apartment</span>
                            <span className="text-xs text-text-secondary font-medium">Orchards</span>
                        </div>
                        <p className="text-2xl font-black text-text-primary">{orchards.length}</p>
                    </div>
                    <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                        <div className="flex items-center gap-2 mb-1.5">
                            <span className="material-symbols-outlined text-indigo-500 text-lg">group</span>
                            <span className="text-xs text-text-secondary font-medium">Active Users</span>
                        </div>
                        <p className="text-2xl font-black text-text-primary">{activeUsers}</p>
                    </div>
                    <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                        <div className="flex items-center gap-2 mb-1.5">
                            <span className="material-symbols-outlined text-amber-500 text-lg">people</span>
                            <span className="text-xs text-text-secondary font-medium">Total Users</span>
                        </div>
                        <p className="text-2xl font-black text-text-primary">{totalUsers}</p>
                    </div>
                    <div className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                        <div className="flex items-center gap-2 mb-1.5">
                            <span className="material-symbols-outlined text-green-500 text-lg">verified</span>
                            <span className="text-xs text-text-secondary font-medium">Compliance</span>
                        </div>
                        <p className="text-2xl font-black text-green-600">98%</p>
                    </div>
                </div>

                {/* Tab Content */}
                <div key={activeTab} className="animate-fade-in">
                    {renderContent()}
                </div>
            </DesktopLayout>

            {/* Setup Wizard Modal */}
            {showWizard && (
                <SetupWizard
                    onComplete={() => { setShowWizard(false); loadData(); }}
                    onCancel={() => setShowWizard(false)}
                />
            )}
        </>
    );
}

/* ── Compliance Card ────────────── */
const ComplianceCard: React.FC<{
    title: string;
    score: number;
    status: string;
    details: string;
}> = ({ title, score, status, details }) => (
    <div className="text-center">
        <div className={`text-3xl font-bold ${score >= 95 ? 'text-green-600' : score >= 80 ? 'text-amber-600' : 'text-red-600'}`}>
            {score}%
        </div>
        <div className="text-sm font-semibold text-text-primary mt-1">{title}</div>
        <div className="text-xs text-text-secondary mt-0.5">{status}</div>
        <div className="text-xs text-text-muted mt-0.5">{details}</div>
    </div>
);
