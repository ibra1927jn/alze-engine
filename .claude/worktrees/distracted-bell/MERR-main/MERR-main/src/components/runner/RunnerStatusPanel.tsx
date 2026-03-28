import React from 'react';
import { RunnerData } from '@/components/modals/RunnerDetailsModal';

interface RunnerStatusPanelProps {
    runner: RunnerData;
    editedRunner: RunnerData;
    isEditing: boolean;
    setEditedRunner: React.Dispatch<React.SetStateAction<RunnerData>>;
    onStatusChange: (status: 'Active' | 'Break' | 'Off Duty') => void;
    onSave: () => void;
    onEdit: () => void;
    onCancelEdit: () => void;
    onDelete: () => void;
}

const RunnerStatusPanel: React.FC<RunnerStatusPanelProps> = ({
    runner, editedRunner, isEditing, setEditedRunner,
    onStatusChange, onSave, onEdit, onCancelEdit, onDelete,
}) => (
    <div className="space-y-4">
        {/* Status Control */}
        <div className="bg-slate-50 rounded-xl p-4 border border-border-light">
            <p className="text-xs font-bold text-text-muted uppercase mb-3">
                Current Status
            </p>
            <div className="grid grid-cols-3 gap-2">
                {(['Active', 'Break', 'Off Duty'] as const).map(status => (
                    <button
                        key={status}
                        onClick={() => onStatusChange(status)}
                        className={`py-2 px-3 rounded-lg text-xs font-bold uppercase transition-all ${editedRunner.status === status
                            ? status === 'Active'
                                ? 'bg-success text-white'
                                : status === 'Break'
                                    ? 'bg-warning text-white'
                                    : 'bg-slate-500 text-white'
                            : 'bg-slate-100 text-text-muted'
                            }`}
                    >
                        {status}
                    </button>
                ))}
            </div>
        </div>

        {/* Row Assignment */}
        <div className="bg-slate-50 rounded-xl p-4 border border-border-light">
            <label className="text-xs font-bold text-text-muted uppercase mb-2 block">
                Assigned Row
            </label>
            {isEditing ? (
                <input
                    type="number"
                    value={editedRunner.currentRow || ''}
                    onChange={e =>
                        setEditedRunner(prev => ({
                            ...prev,
                            currentRow: e.target.value
                                ? parseInt(e.target.value)
                                : undefined,
                        }))
                    }
                    className="w-full px-4 py-2 rounded-lg border-2 border-border-light focus:border-primary outline-none transition-colors"
                    placeholder="Row number"
                />
            ) : (
                <p className="text-lg font-bold text-text-main">
                    {editedRunner.currentRow
                        ? `Row ${editedRunner.currentRow}`
                        : 'Not assigned'}
                </p>
            )}
        </div>

        {/* Stats */}
        <div className="bg-primary/5 rounded-xl p-4 border border-primary/10">
            <p className="text-xs font-bold text-primary uppercase mb-3">
                Today's Performance
            </p>
            <div className="grid grid-cols-2 gap-3">
                <div>
                    <p className="text-2xl font-black text-text-main">
                        {runner.bucketsHandled}
                    </p>
                    <p className="text-xs text-primary/70 font-medium">
                        Buckets Handled
                    </p>
                </div>
                <div>
                    <p className="text-2xl font-black text-text-main">
                        {runner.binsCompleted}
                    </p>
                    <p className="text-xs text-primary/70 font-medium">
                        Bins Completed
                    </p>
                </div>
            </div>
        </div>

        {/* Actions */}
        <div className="space-y-2">
            {isEditing ? (
                <>
                    <button
                        onClick={onSave}
                        className="w-full py-3 gradient-primary glow-primary text-white rounded-xl font-bold active:scale-95 transition-transform"
                    >
                        Save Changes
                    </button>
                    <button
                        onClick={onCancelEdit}
                        className="w-full py-3 bg-slate-200 text-text-sub rounded-xl font-bold hover:bg-slate-300 transition-colors"
                    >
                        Cancel
                    </button>
                </>
            ) : (
                <>
                    <button
                        onClick={onEdit}
                        className="w-full py-3 bg-slate-200 text-text-sub rounded-xl font-bold flex items-center justify-center gap-2 hover:bg-slate-300 transition-colors"
                    >
                        <span className="material-symbols-outlined text-[20px]">
                            edit
                        </span>
                        Edit Details
                    </button>
                    <button
                        onClick={onDelete}
                        className="w-full py-3 bg-red-50 text-danger border-2 border-red-200 rounded-xl font-bold flex items-center justify-center gap-2"
                    >
                        <span className="material-symbols-outlined text-[20px]">
                            delete
                        </span>
                        Remove Runner
                    </button>
                </>
            )}
        </div>
    </div>
);

export default RunnerStatusPanel;
