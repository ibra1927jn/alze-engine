/**
 * DayConfigModal - Modal para configurar el día de trabajo
 * Usado en TeamLeader para configurar orchard, variety y bin type
 */

import React, { useState } from 'react';
import ModalOverlay from '@/components/ui/ModalOverlay';

export interface DayConfig {
    orchard: string;
    variety: string;
    targetSize: string;
    targetColor: string;
    binType: 'Standard' | 'Export' | 'Process';
}

interface DayConfigModalProps {
    config: DayConfig;
    onClose: () => void;
    onSave: (config: DayConfig) => void;
}

const DayConfigModal: React.FC<DayConfigModalProps> = ({ config, onClose, onSave }) => {
    const [editedConfig, setEditedConfig] = useState({ ...config });

    return (
        <ModalOverlay onClose={onClose}>
            <div className="p-6 max-h-[85vh] overflow-y-auto">
                <div className="flex items-center justify-between mb-6">
                    <h3 className="text-xl font-black text-text-main">Day Configuration</h3>
                    <button onClick={onClose} className="text-text-muted hover:text-text-main transition-colors">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>
                <div className="space-y-4">
                    <div>
                        <label className="text-xs font-bold text-text-muted uppercase mb-2 block">Orchard Block</label>
                        <select
                            value={editedConfig.orchard}
                            onChange={(e) => setEditedConfig({ ...editedConfig, orchard: e.target.value })}
                            aria-label="Orchard Block"
                            className="w-full px-4 py-3 rounded-xl border-2 border-border-light focus:border-primary outline-none text-text-main bg-white transition-colors"
                        >
                            <option>El Pedregal - Block 4B</option>
                            <option>Sunny Hills - Block 2A</option>
                            <option>Mountain View - Block 1C</option>
                            <option>Valley Farm - Block 3D</option>
                        </select>
                    </div>
                    <div>
                        <label className="text-xs font-bold text-text-muted uppercase mb-2 block">Variety</label>
                        <select
                            value={editedConfig.variety}
                            onChange={(e) => setEditedConfig({ ...editedConfig, variety: e.target.value })}
                            aria-label="Variety"
                            className="w-full px-4 py-3 rounded-xl border-2 border-border-light focus:border-primary outline-none text-text-main bg-white transition-colors"
                        >
                            <option>Lapin</option>
                            <option>Santina</option>
                            <option>Sweetheart</option>
                            <option>Rainier</option>
                            <option>Bing</option>
                        </select>
                    </div>
                    <div>
                        <label className="text-xs font-bold text-text-muted uppercase mb-2 block">Bin Type</label>
                        <div className="grid grid-cols-3 gap-3">
                            {(['Standard', 'Export', 'Process'] as const).map(type => (
                                <label key={type} className="cursor-pointer">
                                    <input
                                        type="radio"
                                        name="binType"
                                        checked={editedConfig.binType === type}
                                        onChange={() => setEditedConfig({ ...editedConfig, binType: type })}
                                        className="peer sr-only"
                                    />
                                    <div className="h-full rounded-xl border-2 border-border-light p-3 peer-checked:border-primary peer-checked:bg-primary-light flex flex-col items-center transition-all">
                                        <span className="material-symbols-outlined text-primary mb-1">
                                            {type === 'Standard' ? 'shopping_basket' : type === 'Export' ? 'inventory_2' : 'recycling'}
                                        </span>
                                        <span className="text-sm font-bold text-text-main">{type}</span>
                                    </div>
                                </label>
                            ))}
                        </div>
                    </div>
                    <div className="bg-primary-light rounded-xl p-4 border border-primary/20">
                        <p className="text-xs font-bold text-primary uppercase mb-2">📋 Quality Standards</p>
                        <div className="grid grid-cols-2 gap-2 text-sm">
                            <div>
                                <span className="text-text-main font-bold">Size:</span>
                                <span className="text-text-sub ml-1">{editedConfig.targetSize || '28mm+'}</span>
                            </div>
                            <div>
                                <span className="text-text-main font-bold">Color:</span>
                                <span className="text-text-sub ml-1">{editedConfig.targetColor || 'Dark Red'}</span>
                            </div>
                        </div>
                    </div>
                </div>
                <button
                    onClick={() => { onSave(editedConfig); onClose(); }}
                    className="w-full mt-6 py-4 gradient-primary glow-primary text-white rounded-xl font-bold uppercase active:scale-95 transition-all"
                >
                    Save Configuration
                </button>
            </div>
        </ModalOverlay>
    );
};

export default DayConfigModal;
