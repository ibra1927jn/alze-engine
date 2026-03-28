import React, { useState, useMemo } from 'react';
import { useHarvestStore as useHarvest } from '@/stores/useHarvestStore';
import { Picker } from '@/types';
import AddPickerModal from '../../modals/AddPickerModal';
import PickerDetailsModal from '../../modals/PickerDetailsModal';

const TeamView = () => {
    const {
        crew,
        addPicker,
        removePicker,
        updatePicker,
        currentUser: appUser // Map currentUser to appUser as expected by component
    } = useHarvest();

    const [isAddModalOpen, setIsAddModalOpen] = useState(false);
    const [selectedPicker, setSelectedPicker] = useState<Picker | null>(null);
    const [showInactive, setShowInactive] = useState(false);

    // Filter Logic - GLOBAL ROSTER
    // We show all pickers assigned to this TL.
    // 'Active' means they are active in the system (not archived), regardless of orchard assignment.
    const displayedCrew = useMemo(() => {
        // Option 1: Filter by current TL if we are in TL mode
        // Note: Managers see 'all leaders' in TeamsView, but TLs see 'their crew' in TeamView.
        // We filter context crew to show only what belongs to this appUser
        return crew.filter(p => {
            const isMe = p.id === appUser?.id;
            const isMyCrew = p.team_leader_id === appUser?.id;
            const isVisible = showInactive || p.status !== 'inactive';

            // TL view usually shows themselves + their crew members
            return isVisible && (isMe || isMyCrew);
        });
    }, [crew, appUser?.id, showInactive]);

    // Calculate stats
    const totalCrew = displayedCrew.length;
    const activeCrew = displayedCrew.filter(p => p.status === 'active').length;
    const pendingCrew = displayedCrew.filter(p => !p.safety_verified).length;

    const handleDelete = async (e: React.MouseEvent, pickerId: string, pickerName: string) => {
        e.stopPropagation();
        if (window.confirm(`Are you sure you want to remove ${pickerName}?`)) {
            await removePicker(pickerId);
        }
    };

    return (
        <div>
            <header className="sticky top-0 z-30 bg-surface-white/95 backdrop-blur-sm border-b border-border-light pb-3 pt-4 shadow-sm">
                <div className="flex items-center justify-between px-4 py-3">
                    <div className="flex items-center gap-3">
                        <div>
                            <h1 className="text-text-main text-lg font-bold leading-tight tracking-tight">Crew Setup</h1>
                            <p className="text-xs text-text-sub font-medium">Harness & ID Assignment</p>
                        </div>
                    </div>

                    <div className="flex items-center gap-2">
                        <label className="flex items-center gap-2 text-xs font-bold text-text-muted uppercase cursor-pointer bg-slate-100 px-3 py-1.5 rounded-full hover:bg-slate-200 transition-colors">
                            <input
                                type="checkbox"
                                checked={showInactive}
                                onChange={(e) => setShowInactive(e.target.checked)}
                                className="accent-primary-vibrant size-4"
                            />
                            History
                        </label>
                    </div>
                </div>
                <div className="grid grid-cols-3 gap-3 px-4 mt-1">
                    <div className="flex flex-col items-center justify-center p-2 rounded-lg bg-white border border-border-light shadow-sm">
                        <span className="text-[10px] text-text-sub uppercase tracking-wider font-semibold">Total Crew</span>
                        <span className="text-text-main text-xl font-bold font-mono tracking-tight">{totalCrew}</span>
                    </div>
                    <div className="flex flex-col items-center justify-center p-2 rounded-lg bg-white border border-border-light shadow-sm">
                        <span className="text-[10px] text-text-sub uppercase tracking-wider font-semibold">Active</span>
                        <span className="text-success text-xl font-bold font-mono tracking-tight">{activeCrew}</span>
                    </div>
                    <div className="flex flex-col items-center justify-center p-2 rounded-lg bg-white border-l-4 border-l-warning border-y border-r border-border-light shadow-sm">
                        <span className="text-[10px] text-text-sub uppercase tracking-wider font-semibold">Pending</span>
                        <span className="text-warning text-xl font-bold font-mono tracking-tight">{pendingCrew}</span>
                    </div>
                </div>
            </header>

            <main className="px-4 mt-6 space-y-3 pb-24">
                {displayedCrew.length === 0 ? (
                    <div className="text-center py-10 opacity-50">
                        <span className="material-symbols-outlined text-4xl mb-2 text-text-muted">group_off</span>
                        <p className="text-sm font-bold text-text-muted">No active pickers found.</p>
                        {!showInactive && <p className="text-xs text-text-muted mt-1">Check "History" to see archived crew.</p>}
                    </div>
                ) : (
                    displayedCrew.map(picker => (
                        <div
                            key={picker.id}
                            onClick={() => setSelectedPicker(picker)}
                            className={`bg-white rounded-xl p-4 border border-border-light shadow-sm relative overflow-hidden group hover:border-primary-vibrant/30 cursor-pointer transition-all active:scale-[0.99] ${picker.status === 'inactive' ? 'opacity-60 grayscale' : ''}`}
                        >
                            <div className="flex justify-between items-start mb-4">
                                <div className="flex items-center gap-3">
                                    <div className="size-12 rounded-full bg-slate-200 flex items-center justify-center font-bold text-text-sub text-lg overflow-hidden">
                                        {picker.avatar || (picker.name || '??').substring(0, 2).toUpperCase()}
                                    </div>
                                    <div>
                                        <div className="flex items-center gap-1.5">
                                            <h3 className="text-text-main font-bold text-base">{picker.name}</h3>
                                            {(picker.total_buckets_today || 0) > 20 &&
                                                <span className="material-symbols-outlined text-bonus text-[16px] fill-current">star</span>
                                            }
                                            {picker.status === 'inactive' &&
                                                <span className="bg-slate-200 text-text-sub text-[10px] px-1.5 py-0.5 rounded uppercase font-bold ml-2">Archived</span>
                                            }
                                        </div>
                                        <p className="text-xs text-text-sub font-medium flex items-center gap-1.5 mt-0.5">
                                            <span className={`px-1.5 py-0.5 rounded text-[10px] font-bold tracking-wide uppercase ${picker.safety_verified ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'}`}>
                                                {picker.safety_verified ? 'Onboarded' : 'Pending'}
                                            </span>
                                            <span className="text-slate-300">|</span>
                                            {!picker.orchard_id ? (
                                                <span className="px-1.5 py-0.5 rounded text-[10px] font-bold tracking-wide uppercase bg-slate-100 text-text-muted border border-border-light">
                                                    On Bench
                                                </span>
                                            ) : (
                                                <span className="text-primary font-bold">Row {picker.current_row || '—'}</span>
                                            )}
                                        </p>
                                    </div>
                                </div>
                                <div className="text-right flex flex-col items-end gap-1">
                                    <div>
                                        <span className="block text-2xl font-black text-slate-800 leading-none">{picker.total_buckets_today || 0}</span>
                                        <span className="text-[10px] text-text-muted font-bold uppercase">Buckets</span>
                                    </div>
                                    <button
                                        onClick={(e) => handleDelete(e, picker.id, picker.name)}
                                        className="size-8 rounded-full bg-red-50 text-red-500 flex items-center justify-center hover:bg-red-100 active:bg-red-200 transition-colors mt-1"
                                        title="Delete/Archive Picker"
                                    >
                                        <span className="material-symbols-outlined text-lg">delete</span>
                                    </button>
                                </div>
                            </div>
                            <div className="grid grid-cols-2 gap-3 bg-background-light/50 p-3 rounded-lg border border-border-light/50">
                                <div>
                                    <label className="text-[10px] uppercase font-bold text-text-sub tracking-wide block mb-1.5">Picker ID</label>
                                    <div className="relative">
                                        <input className="w-full bg-white border-border-light rounded-lg px-3 py-2 text-sm font-mono font-bold text-text-main pointer-events-none" type="text" readOnly value={picker.picker_id} aria-label="Picker ID" />
                                    </div>
                                </div>
                                <div>
                                    <label className="text-[10px] uppercase font-bold text-primary-dim tracking-wide block mb-1.5">Harness No.</label>
                                    <div className="relative">
                                        <input
                                            className="w-full bg-white border border-border-light rounded-lg px-3 py-2 text-sm font-mono font-bold text-primary-vibrant uppercase focus:border-primary focus:ring-1 focus:ring-primary outline-none transition-all"
                                            type="text"
                                            defaultValue={picker.harness_id || ''}
                                            placeholder="Assign..."
                                            onBlur={(e) => {
                                                if (e.target.value !== picker.harness_id) {
                                                    updatePicker(picker.id, { harness_id: e.target.value.toUpperCase() });
                                                }
                                            }}
                                            onKeyDown={(e) => {
                                                if (e.key === 'Enter') {
                                                    e.currentTarget.blur();
                                                }
                                            }}
                                            onClick={(e) => e.stopPropagation()}
                                        />
                                    </div>
                                </div>
                            </div>
                        </div>
                    ))
                )}

                <div className="fixed bottom-24 left-0 w-full px-4 pb-2 z-40 pointer-events-none">
                    <div className="pointer-events-auto">
                        <button
                            onClick={() => setIsAddModalOpen(true)}
                            className="w-full bg-primary-vibrant hover:bg-primary-dim text-white text-lg font-bold py-3.5 rounded-xl shadow-[0_8px_20px_rgba(255,31,61,0.4)] flex items-center justify-center gap-2 transform active:scale-[0.98] transition-all border border-white/10"
                        >
                            <span className="material-symbols-outlined text-[24px]">person_add</span>
                            Add New Picker
                        </button>
                    </div>
                </div>
            </main>

            {isAddModalOpen && (
                <AddPickerModal
                    onClose={() => setIsAddModalOpen(false)}
                    onAdd={addPicker}
                />
            )}

            {selectedPicker && (
                <PickerDetailsModal
                    picker={selectedPicker}
                    onClose={() => setSelectedPicker(null)}
                    onDelete={removePicker}
                    onUpdate={updatePicker}
                    allCrew={displayedCrew}
                />
            )}
        </div>
    );
};

export default TeamView;
