import React, { useState } from 'react';
import { RegisteredUser } from '../../services/database.service';
import ModalOverlay from '@/components/ui/ModalOverlay';

interface RunnerSelectionModalProps {
    availableRunners: RegisteredUser[];
    selectedRunnerIds: string[];
    onClose: () => void;
    onSave: (ids: string[]) => void;
}

const RunnerSelectionModal: React.FC<RunnerSelectionModalProps> = ({
    availableRunners,
    selectedRunnerIds,
    onClose,
    onSave
}) => {
    const [selected, setSelected] = useState<string[]>(selectedRunnerIds);

    const toggleRunner = (id: string) => {
        setSelected(prev =>
            prev.includes(id) ? prev.filter(r => r !== id) : [...prev, id]
        );
    };

    return (
        <ModalOverlay onClose={onClose}>
            <div className="max-h-[85vh] overflow-y-auto">
                <div className="flex items-center justify-between p-6 border-b border-border-light">
                    <h3 className="text-xl font-black text-text-main">Select Active Runners</h3>
                    <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                <div className="p-6 space-y-2">
                    {availableRunners.map(runner => (
                        <div
                            key={runner.id}
                            onClick={() => toggleRunner(runner.id)}
                            className={`p-4 rounded-xl border flex items-center justify-between cursor-pointer transition-all ${selected.includes(runner.id)
                                ? 'bg-primary/5 border-primary'
                                : 'bg-slate-50 border-border-light hover:border-slate-300'
                                }`}
                        >
                            <div className="flex items-center gap-3">
                                <div className={`size-10 rounded-full flex items-center justify-center font-bold text-white ${selected.includes(runner.id) ? 'bg-primary' : 'bg-slate-300'
                                    }`}>
                                    {runner.full_name?.substring(0, 2).toUpperCase()}
                                </div>
                                <div>
                                    <p className="text-text-main font-bold">{runner.full_name}</p>
                                    <p className="text-xs text-text-muted">{runner.email}</p>
                                </div>
                            </div>
                            {selected.includes(runner.id) && (
                                <span className="material-symbols-outlined text-primary">check_circle</span>
                            )}
                        </div>
                    ))}
                    {availableRunners.length === 0 && (
                        <p className="text-center text-text-muted py-8">No runners found.</p>
                    )}
                </div>

                <div className="p-6 pt-0">
                    <button
                        onClick={() => { onSave(selected); onClose(); }}
                        className="w-full py-4 gradient-primary glow-primary text-white rounded-xl font-bold uppercase tracking-widest active:scale-[0.98] transition-all"
                    >
                        Confirm Selection ({selected.length})
                    </button>
                </div>
            </div>
        </ModalOverlay>
    );
};

export default RunnerSelectionModal;
