import React from 'react';
import { RegisteredUser } from '../../services/database.service';
import { Picker, MINIMUM_WAGE, PIECE_RATE } from '../../types';
import ModalOverlay from '@/components/ui/ModalOverlay';

interface TeamDetailsModalProps {
    leader: RegisteredUser;
    teamMembers: Picker[];
    onClose: () => void;
}

const TeamDetailsModal: React.FC<TeamDetailsModalProps> = ({
    leader,
    teamMembers,
    onClose
}) => {
    const totalBuckets = teamMembers.reduce((sum, p) => sum + p.total_buckets_today, 0);

    return (
        <ModalOverlay onClose={onClose} maxWidth="max-w-2xl">
            <div className="max-h-[85vh] overflow-y-auto">
                {/* Header */}
                <div className="flex items-center justify-between p-6 border-b border-border-light">
                    <div className="flex items-center gap-4">
                        <div className="size-16 rounded-full bg-primary/10 flex items-center justify-center font-bold text-primary text-2xl">
                            {leader.full_name?.substring(0, 2).toUpperCase()}
                        </div>
                        <div>
                            <h3 className="text-2xl font-black text-text-main">{leader.full_name}'s Team</h3>
                            <div className="flex items-center gap-3 mt-1">
                                <span className="text-sm text-text-muted">{teamMembers.length} Members</span>
                                <span className="w-1 h-1 rounded-full bg-border-light"></span>
                                <span className="text-sm text-primary font-bold">{totalBuckets} Total Buckets</span>
                            </div>
                        </div>
                    </div>
                    <button onClick={onClose} className="text-text-muted hover:text-text-main bg-slate-100 p-2 rounded-full transition-colors">
                        <span className="material-symbols-outlined">close</span>
                    </button>
                </div>

                {/* Team Members List */}
                <div className="p-6 space-y-1">
                    <div className="grid grid-cols-12 gap-2 text-xs font-bold text-text-muted uppercase px-4 pb-2">
                        <div className="col-span-5">Picker</div>
                        <div className="col-span-2 text-center">Buckets</div>
                        <div className="col-span-3 text-center">Performance</div>
                        <div className="col-span-2 text-right">Row</div>
                    </div>

                    {teamMembers.length === 0 ? (
                        <div className="text-center py-10 text-text-muted">
                            <span className="material-symbols-outlined text-4xl mb-2">group_off</span>
                            <p>No pickers assigned to this team.</p>
                        </div>
                    ) : (
                        teamMembers.map(picker => {
                            const hourlyRate = picker.hours && picker.hours > 0
                                ? (picker.total_buckets_today * PIECE_RATE) / picker.hours
                                : 0;
                            const isAboveMin = hourlyRate >= MINIMUM_WAGE;

                            return (
                                <div key={picker.id} className="grid grid-cols-12 gap-2 items-center bg-slate-50 p-3 rounded-xl border border-border-light">
                                    {/* Name & ID */}
                                    <div className="col-span-5 flex items-center gap-3">
                                        <div className="size-8 rounded-full bg-white border border-border-light flex items-center justify-center text-xs font-bold text-text-sub">
                                            {picker.avatar}
                                        </div>
                                        <div>
                                            <p className="text-text-main font-bold text-sm truncate">{picker.name}</p>
                                            <p className="text-[10px] text-text-muted">{picker.picker_id}</p>
                                        </div>
                                    </div>

                                    {/* Buckets */}
                                    <div className="col-span-2 text-center">
                                        <span className="text-text-main font-black text-lg">{picker.total_buckets_today}</span>
                                    </div>

                                    {/* Rate */}
                                    <div className="col-span-3 text-center">
                                        <div className={`text-xs font-bold ${isAboveMin ? 'text-success' : 'text-danger'}`}>
                                            ${hourlyRate.toFixed(2)}/hr
                                        </div>
                                        <div className="text-[10px] text-text-muted">
                                            {picker.hours ? `${picker.hours.toFixed(1)}h` : '0h'}
                                        </div>
                                    </div>

                                    {/* Row */}
                                    <div className="col-span-2 text-right">
                                        <span className="bg-slate-100 text-text-sub px-2 py-1 rounded text-xs font-bold">
                                            {picker.current_row ? `R${picker.current_row}` : '--'}
                                        </span>
                                    </div>
                                </div>
                            );
                        })
                    )}
                </div>
            </div>
        </ModalOverlay>
    );
};

export default TeamDetailsModal;
