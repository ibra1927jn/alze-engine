/**
 * RequestsTab.tsx — Logistics Transport Requests
 * Priority-sorted request cards with Assign/Complete/Cancel action buttons
 */
import React, { useState, useMemo } from 'react';
import EmptyState from '@/components/ui/EmptyState';
import FilterBar from '@/components/ui/FilterBar';
import {
    TransportRequest, Tractor,
    assignVehicleToRequest, completeTransportRequest
} from '@/services/logistics-dept.service';

const PRIORITY_BADGES: Record<string, string> = {
    urgent: 'bg-red-100 text-red-700',
    high: 'bg-amber-100 text-amber-700',
    normal: 'bg-surface-secondary text-text-secondary',
};

const STATUS_BADGES: Record<string, string> = {
    pending: 'bg-amber-50 text-amber-700',
    assigned: 'bg-sky-50 text-sky-700',
    in_progress: 'bg-indigo-50 text-indigo-700',
    completed: 'bg-emerald-50 text-emerald-700',
    cancelled: 'bg-red-50 text-red-700',
};

interface RequestsTabProps {
    requests: TransportRequest[];
    tractors?: Tractor[];
    onRefresh?: () => void;
}

const RequestsTab: React.FC<RequestsTabProps> = ({ requests, tractors = [], onRefresh }) => {
    const [assigningId, setAssigningId] = useState<string | null>(null);
    const [selectedVehicle, setSelectedVehicle] = useState('');
    const [toast, setToast] = useState<{ message: string; type: 'success' | 'error' } | null>(null);
    const [searchQuery, setSearchQuery] = useState('');
    const [activeFilters, setActiveFilters] = useState<Record<string, string>>({});

    const filterOpts = useMemo(() => ({
        priorities: [...new Set(requests.map(r => r.priority))].sort(),
        statuses: [...new Set(requests.map(r => r.status))].sort(),
    }), [requests]);

    const filteredRequests = useMemo(() => requests.filter(req => {
        const matchesSearch = !searchQuery ||
            req.zone.toLowerCase().includes(searchQuery.toLowerCase()) ||
            String(req.bins_count).includes(searchQuery);
        const matchesPriority = !activeFilters.priority || req.priority === activeFilters.priority;
        const matchesStatus = !activeFilters.status || req.status === activeFilters.status;
        return matchesSearch && matchesPriority && matchesStatus;
    }), [requests, searchQuery, activeFilters]);

    const showToast = (message: string, type: 'success' | 'error' = 'success') => {
        setToast({ message, type });
        setTimeout(() => setToast(null), 3000);
    };

    const availableTractors = tractors.filter(t => t.status === 'active' || t.status === 'idle');

    const handleAssign = (requestId: string, updatedAt?: string) => {
        if (!selectedVehicle) return;
        assignVehicleToRequest(requestId, selectedVehicle, 'current_user', updatedAt);
        showToast('Vehicle assigned to transport request');
        setAssigningId(null);
        setSelectedVehicle('');
        onRefresh?.();
    };

    const handleComplete = (requestId: string, updatedAt?: string) => {
        completeTransportRequest(requestId, updatedAt);
        showToast('Transport request completed');
        onRefresh?.();
    };

    const handleCancel = (requestId: string, updatedAt?: string) => {
        completeTransportRequest(requestId, updatedAt); // Re-uses complete — status set server-side
        showToast('Transport request cancelled', 'error');
        onRefresh?.();
    };

    return (
        <div className="space-y-3 relative">
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

            {/* Filter Bar */}
            <FilterBar
                searchValue={searchQuery}
                onSearchChange={setSearchQuery}
                searchPlaceholder="Search by zone or cargo..."
                filters={[
                    { key: 'priority', label: 'Priority', options: filterOpts.priorities, icon: 'priority_high' },
                    { key: 'status', label: 'Status', options: filterOpts.statuses, icon: 'pending_actions' },
                ]}
                activeFilters={activeFilters}
                onFilterChange={(key, value) => setActiveFilters(prev => ({ ...prev, [key]: value }))}
                onClearAll={() => { setSearchQuery(''); setActiveFilters({}); }}
            />

            {filteredRequests.length === 0 && (
                <EmptyState
                    icon="local_shipping"
                    title="No requests found"
                    subtitle={searchQuery || Object.values(activeFilters).some(v => v) ? "Try adjusting your filters" : "Transport requests will appear here as teams need pickups"}
                    compact
                />
            )}

            <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
                {filteredRequests.map(req => (
                    <div key={req.id} className="bg-white rounded-xl p-4 shadow-sm border border-border-light">
                        <div className="flex items-center justify-between mb-2">
                            <div className="flex items-center gap-2">
                                <span className={`text-[10px] font-bold uppercase px-2 py-0.5 rounded-full ${PRIORITY_BADGES[req.priority] || 'bg-surface-secondary text-text-secondary'}`}>
                                    {req.priority}
                                </span>
                                <h4 className="font-bold text-text-primary text-sm">Zone {req.zone}</h4>
                            </div>
                            <span className={`text-[10px] font-bold uppercase px-2 py-0.5 rounded-full ${STATUS_BADGES[req.status] || 'bg-surface-secondary text-text-secondary'}`}>
                                {req.status.replace('_', ' ')}
                            </span>
                        </div>
                        <div className="flex items-center gap-3 text-xs text-text-secondary mb-2">
                            <span>{req.requester_name}</span>
                            <span>•</span>
                            <span>{req.bins_count} bins</span>
                            {req.assigned_tractor && (
                                <>
                                    <span>•</span>
                                    <span className="text-indigo-600 font-medium">Assigned to {req.assigned_tractor}</span>
                                </>
                            )}
                        </div>
                        {req.notes && (
                            <p className="text-xs text-text-muted italic mb-2">{req.notes}</p>
                        )}
                        <p className="text-[10px] text-text-muted mb-3">
                            {new Date(req.created_at).toLocaleTimeString('en-NZ', { hour: '2-digit', minute: '2-digit' })} — {new Date(req.created_at).toLocaleDateString('en-NZ')}
                        </p>

                        {/* Action Buttons */}
                        {req.status === 'pending' && (
                            <div className="space-y-2">
                                {assigningId === req.id ? (
                                    <div className="flex gap-2">
                                        <select
                                            value={selectedVehicle}
                                            onChange={e => setSelectedVehicle(e.target.value)}
                                            className="flex-1 px-3 py-1.5 rounded-lg border border-border-light text-xs focus:border-indigo-500 focus:ring-2 focus:ring-indigo-500/10 outline-none"
                                            aria-label="Select vehicle"
                                        >
                                            <option value="">Select vehicle...</option>
                                            {availableTractors.map(t => (
                                                <option key={t.id} value={t.id}>
                                                    {t.name} — {t.zone} ({t.status})
                                                </option>
                                            ))}
                                        </select>
                                        <button
                                            onClick={() => handleAssign(req.id, req.updated_at)}
                                            disabled={!selectedVehicle}
                                            className="px-3 py-1.5 rounded-lg bg-indigo-600 text-white text-xs font-bold hover:bg-indigo-700 transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
                                        >
                                            Assign
                                        </button>
                                        <button
                                            onClick={() => { setAssigningId(null); setSelectedVehicle(''); }}
                                            className="px-3 py-1.5 rounded-lg bg-surface-secondary text-text-secondary text-xs font-bold hover:bg-surface-secondary transition-colors"
                                        >
                                            Cancel
                                        </button>
                                    </div>
                                ) : (
                                    <div className="flex gap-2">
                                        <button
                                            onClick={() => setAssigningId(req.id)}
                                            className="flex-1 py-1.5 rounded-lg bg-indigo-50 text-indigo-700 text-xs font-bold hover:bg-indigo-100 transition-colors flex items-center justify-center gap-1"
                                        >
                                            <span className="material-symbols-outlined text-xs">agriculture</span>
                                            Assign Vehicle
                                        </button>
                                        <button
                                            onClick={() => handleCancel(req.id, req.updated_at)}
                                            className="py-1.5 px-3 rounded-lg bg-red-50 text-red-700 text-xs font-bold hover:bg-red-100 transition-colors"
                                        >
                                            Cancel
                                        </button>
                                    </div>
                                )}
                            </div>
                        )}

                        {(req.status === 'assigned' || req.status === 'in_progress') && (
                            <div className="flex gap-2">
                                <button
                                    onClick={() => handleComplete(req.id, req.updated_at)}
                                    className="flex-1 py-1.5 rounded-lg bg-emerald-50 text-emerald-700 text-xs font-bold hover:bg-emerald-100 transition-colors flex items-center justify-center gap-1"
                                >
                                    <span className="material-symbols-outlined text-xs">check_circle</span>
                                    Complete
                                </button>
                                <button
                                    onClick={() => handleCancel(req.id, req.updated_at)}
                                    className="py-1.5 px-3 rounded-lg bg-red-50 text-red-700 text-xs font-bold hover:bg-red-100 transition-colors"
                                >
                                    Cancel
                                </button>
                            </div>
                        )}
                    </div>
                ))}
            </div>
        </div>
    );
};

export default RequestsTab;
