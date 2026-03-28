/**
 * RunnerProfileView — Runner-specific profile: activity stats and details/edit
 */
import React, { useState } from 'react';
import { Picker, PickerStatus } from '../../../types';

interface RunnerProfileViewProps {
    picker: Picker;
    onUpdate: (id: string, updates: Partial<Picker>) => void;
}

const RunnerProfileView: React.FC<RunnerProfileViewProps> = React.memo(({
    picker,
    onUpdate,
}) => {
    const [isEditing, setIsEditing] = useState(false);
    const [status, setStatus] = useState<PickerStatus>(picker.status);

    const handleSave = () => {
        onUpdate(picker.id, { status });
        setIsEditing(false);
    };

    return (
        <>
            {/* Activity Stats */}
            <div className="bg-white rounded-2xl border border-slate-100 shadow-md p-5">
                <p className="text-xs font-bold uppercase tracking-wider text-slate-400 mb-4">Today&apos;s Activity</p>
                <div className="grid grid-cols-2 gap-3">
                    <div className="bg-amber-50 rounded-xl p-4 text-center">
                        <p className="text-3xl font-black text-amber-700">{picker.total_buckets_today}</p>
                        <p className="text-[11px] text-amber-600 font-medium mt-0.5">Buckets Collected</p>
                    </div>
                    <div className="bg-slate-50 rounded-xl p-4 text-center">
                        <p className="text-3xl font-black text-slate-900">{picker.hours?.toFixed(1) || '0'}h</p>
                        <p className="text-[11px] text-slate-500 font-medium mt-0.5">Hours On-Site</p>
                    </div>
                </div>
            </div>

            {/* Details Card */}
            <div className="bg-white rounded-2xl border border-slate-100 shadow-sm p-5">
                <div className="flex items-center justify-between mb-3">
                    <p className="text-xs font-bold uppercase tracking-wider text-slate-400">Details</p>
                    {!isEditing && <button onClick={() => setIsEditing(true)} className="text-amber-600 text-xs font-bold hover:text-amber-800 transition-colors flex items-center gap-1"><span className="material-symbols-outlined text-[14px]">edit</span>Edit</button>}
                </div>
                {isEditing ? (
                    <div className="space-y-3">
                        <div>
                            <label className="text-slate-500 text-xs block mb-1">Status</label>
                            <select value={status} onChange={(e) => setStatus(e.target.value as PickerStatus)} aria-label="Status"
                                className="w-full px-4 py-2.5 rounded-xl border border-slate-200 focus:border-amber-400 focus:ring-2 focus:ring-amber-100 outline-none text-slate-900 bg-white transition-all">
                                <option value="active">Active</option>
                                <option value="on_break">On Break</option>
                                <option value="inactive">Inactive</option>
                            </select>
                        </div>
                        <div className="flex gap-2 pt-1">
                            <button onClick={handleSave} className="flex-1 py-2.5 bg-amber-600 hover:bg-amber-700 text-white rounded-xl font-bold text-sm transition-colors">Save</button>
                            <button onClick={() => setIsEditing(false)} className="flex-1 py-2.5 bg-slate-100 text-slate-600 rounded-xl font-bold text-sm hover:bg-slate-200 transition-colors">Cancel</button>
                        </div>
                    </div>
                ) : (
                    <div className="grid grid-cols-2 gap-3">
                        <div className="bg-slate-50 rounded-xl p-3"><p className="text-[11px] text-slate-400 font-medium mb-0.5">Current Row</p><p className="text-sm font-bold text-slate-900">{picker.current_row ? `Row ${picker.current_row}` : 'Not assigned'}</p></div>
                        <div className="bg-slate-50 rounded-xl p-3"><p className="text-[11px] text-slate-400 font-medium mb-0.5">Assigned Team</p><p className="text-sm font-bold text-slate-900">{picker.team_leader_id ? 'Assigned' : 'Unassigned'}</p></div>
                    </div>
                )}
            </div>
        </>
    );
});

RunnerProfileView.displayName = 'RunnerProfileView';
export default RunnerProfileView;
