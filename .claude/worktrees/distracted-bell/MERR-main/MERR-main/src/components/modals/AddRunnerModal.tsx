/**
 * Add Runner Modal - For adding new bucket runners
 */
import React, { useState } from 'react';
import ModalOverlay from '@/components/ui/ModalOverlay';

export interface RunnerData {
    id: string;
    name: string;
    avatar: string;
    status: 'Active' | 'Break' | 'Off Duty';
    startTime: string;
    breakTime?: string;
    currentRow?: number;
    bucketsHandled: number;
    binsCompleted: number;
}

interface AddRunnerModalProps {
    onClose: () => void;
    onAdd: (runner: RunnerData) => void;
}

const AddRunnerModal: React.FC<AddRunnerModalProps> = ({ onClose, onAdd }) => {
    const [name, setName] = useState('');
    const [startTime, setStartTime] = useState('');
    const [currentRow, setCurrentRow] = useState('');

    const handleAdd = () => {
        if (name && startTime) {
            const newRunner: RunnerData = {
                id: `RUNNER-${Date.now()}`,
                name,
                avatar: name.charAt(0).toUpperCase(),
                status: 'Active',
                startTime,
                currentRow: currentRow ? parseInt(currentRow) : undefined,
                bucketsHandled: 0,
                binsCompleted: 0,
            };
            onAdd(newRunner);
            onClose();
        }
    };

    return (
        <ModalOverlay onClose={onClose}>
            <div className="p-6">
                <div className="flex items-center justify-between mb-6">
                    <h3 className="text-xl font-black text-text-main">Add New Runner</h3>
                    <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                <div className="space-y-4">
                    <div>
                        <label className="text-xs font-bold text-text-muted uppercase mb-2 block">
                            Full Name *
                        </label>
                        <input
                            type="text"
                            value={name}
                            onChange={e => setName(e.target.value)}
                            placeholder="e.g. John Smith"
                            className="w-full px-4 py-3 rounded-xl border-2 border-border-light focus:border-primary outline-none transition-colors"
                        />
                    </div>

                    <div>
                        <label className="text-xs font-bold text-text-muted uppercase mb-2 block">
                            Start Time *
                        </label>
                        <input
                            type="time"
                            value={startTime}
                            onChange={e => setStartTime(e.target.value)}
                            aria-label="Start Time"
                            className="w-full px-4 py-3 rounded-xl border-2 border-border-light focus:border-primary outline-none transition-colors"
                        />
                    </div>

                    <div>
                        <label className="text-xs font-bold text-text-muted uppercase mb-2 block">
                            Assigned Row (Optional)
                        </label>
                        <input
                            type="number"
                            value={currentRow}
                            onChange={e => setCurrentRow(e.target.value)}
                            placeholder="e.g. 12"
                            className="w-full px-4 py-3 rounded-xl border-2 border-border-light focus:border-primary outline-none transition-colors"
                        />
                    </div>

                    <div className="bg-primary-light rounded-xl p-4 border border-primary/20">
                        <p className="text-xs font-bold text-primary uppercase mb-1">📋 Initial Status</p>
                        <p className="text-sm text-text-main">
                            Will be set to <strong>Active</strong> upon creation
                        </p>
                    </div>
                </div>

                <button
                    onClick={handleAdd}
                    disabled={!name || !startTime}
                    className="w-full mt-6 py-4 gradient-primary glow-primary text-white rounded-xl font-bold uppercase tracking-widest disabled:bg-surface-tertiary disabled:shadow-none active:scale-95 transition-all"
                >
                    Add Runner
                </button>
            </div>
        </ModalOverlay>
    );
};

export default AddRunnerModal;
