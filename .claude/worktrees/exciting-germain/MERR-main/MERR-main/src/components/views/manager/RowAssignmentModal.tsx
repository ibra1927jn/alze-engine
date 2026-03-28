import React, { useState, useMemo } from 'react';
import { useHarvestStore as useHarvest } from '@/stores/useHarvestStore';
import { Picker } from '@/types';

interface RowAssignmentModalProps {
    onClose: () => void;
    initialRow?: number;
    onViewPicker?: (picker: Picker) => void;
}

interface TeamOnRow {
    leader: Picker | null;
    members: Picker[];
    total: number;
}

const RowAssignmentModal: React.FC<RowAssignmentModalProps> = ({ onClose, initialRow = 1, onViewPicker }) => {
    const { assignRows, crew, orchard, rowAssignments } = useHarvest();
    const orchardBlocks = useHarvest(s => s.orchardBlocks);
    const selectedBlockId = useHarvest(s => s.selectedBlockId);
    const selectedVariety = useHarvest(s => s.selectedVariety);
    const [selectedRows, setSelectedRows] = useState<number[]>([initialRow]);
    const [selectedLeader, setSelectedLeader] = useState<string>('');
    const [selectedSide, setSelectedSide] = useState<'north' | 'south'>('north');
    const [assigning, setAssigning] = useState(false);

    const teamLeaders = crew.filter(p => p.role === 'team_leader');

    // Block-specific rows
    const selectedBlock = orchardBlocks.find(b => b.id === selectedBlockId) || null;
    const blockRows = useMemo(() => {
        if (selectedBlock) {
            return Array.from(
                { length: selectedBlock.totalRows },
                (_, i) => selectedBlock.startRow + i
            );
        }
        return Array.from({ length: 20 }, (_, i) => i + 1);
    }, [selectedBlock]);

    // Build teams per row from rowAssignments (supports multiple teams per row)
    const teamsPerRow = useMemo(() => {
        const map: Record<number, TeamOnRow[]> = {};
        rowAssignments.forEach(ra => {
            if (!map[ra.row_number]) map[ra.row_number] = [];
            const team: TeamOnRow = { leader: null, members: [], total: 0 };
            ra.assigned_pickers.forEach(pid => {
                const p = crew.find(c => c.id === pid);
                if (!p) return;
                // Team leader goes as leader, everyone else (pickers, runners) as members
                if (p.role === 'team_leader' && !team.leader) {
                    team.leader = p;
                } else {
                    team.members.push(p);
                }
            });
            team.total = (team.leader ? 1 : 0) + team.members.length;
            if (team.total > 0) map[ra.row_number].push(team);
        });
        return map;
    }, [rowAssignments, crew]);

    const teamsOnRow = teamsPerRow[initialRow] || [];
    const totalPeopleOnRow = teamsOnRow.reduce((s, t) => s + t.total, 0);
    const occupiedCount = blockRows.filter(r => (teamsPerRow[r] || []).length > 0).length;

    // Unique rows a leader is assigned to
    const getLeaderRows = (leaderId: string): number[] => {
        const rows = rowAssignments
            .filter(ra => ra.assigned_pickers.includes(leaderId))
            .map(ra => ra.row_number);
        return [...new Set(rows)].sort((a, b) => a - b);
    };

    const toggleRow = (row: number) => {
        setSelectedRows(prev =>
            prev.includes(row) ? prev.filter(r => r !== row) : [...prev, row]
        );
    };

    const handleAssign = async () => {
        if (!assignRows || !selectedLeader || selectedRows.length === 0) return;
        setAssigning(true);
        // Gather ALL team members: leader + pickers + runners
        const teamMembers = crew.filter(p => p.team_leader_id === selectedLeader || p.id === selectedLeader);
        const memberIds = teamMembers.map(p => p.id);
        // ONE batch call — no loop, no sequential clobbering
        await assignRows(selectedRows, selectedSide, memberIds);
        setAssigning(false);
        onClose();
    };

    return (
        <div className="fixed inset-0 z-[60] flex items-center justify-center bg-black/50 backdrop-blur-sm p-4" onClick={onClose}>
            <div
                className="bg-white w-full max-w-sm rounded-2xl shadow-2xl relative overflow-hidden"
                onClick={(e) => e.stopPropagation()}
            >
                {/* Header */}
                <div className="px-5 pt-5 pb-3 border-b border-slate-100">
                    <div className="flex items-center justify-between">
                        <div>
                            <h2 className="text-lg font-black text-text-main">Row {initialRow}</h2>
                            <p className="text-xs text-slate-400 mt-0.5">
                                {selectedBlock?.name || orchard?.name || 'Orchard'}
                                {' · '}{occupiedCount}/{blockRows.length} rows assigned
                            </p>
                        </div>
                        <button onClick={onClose} className="w-8 h-8 rounded-full bg-slate-100 flex items-center justify-center hover:bg-slate-200 transition-colors">
                            <span className="material-symbols-outlined text-slate-500 text-lg">close</span>
                        </button>
                    </div>
                </div>

                <div className="px-5 py-4 space-y-4 max-h-[65vh] overflow-y-auto">

                    {/* Teams on this row */}
                    {teamsOnRow.length > 0 ? (
                        <div className="space-y-2">
                            <div className="flex items-center justify-between">
                                <p className="text-[10px] font-bold text-amber-500 uppercase">
                                    {teamsOnRow.length} team{teamsOnRow.length > 1 ? 's' : ''} on Row {initialRow}
                                </p>
                                <span className="text-[10px] font-bold text-amber-400">{totalPeopleOnRow} people</span>
                            </div>
                            {teamsOnRow.map((team, idx) => (
                                <div key={idx} className="p-3 bg-amber-50 rounded-xl border border-amber-100">
                                    {team.leader && (
                                        <div className="flex items-center gap-2.5">
                                            <div
                                                className={`w-8 h-8 rounded-full bg-slate-800 flex items-center justify-center text-white text-[10px] font-bold flex-shrink-0 ${onViewPicker ? 'cursor-pointer hover:ring-2 ring-amber-400' : ''}`}
                                                onClick={() => onViewPicker && team.leader && onViewPicker(team.leader)}
                                            >
                                                {team.leader.name.split(' ').map(n => n[0]).join('').slice(0, 2).toUpperCase()}
                                            </div>
                                            <div className="flex-1 min-w-0">
                                                <p className="text-sm font-bold text-amber-900 truncate">{team.leader.name}</p>
                                                <p className="text-[10px] text-amber-600">
                                                    {team.total} people · {(team.leader.total_buckets_today || 0) + team.members.reduce((s, m) => s + (m.total_buckets_today || 0), 0)} buckets
                                                </p>
                                            </div>
                                        </div>
                                    )}
                                    {team.members.length > 0 && (
                                        <div className="space-y-2 mt-2">
                                            {team.members.map(m => {
                                                const memberIsRunner = m.role === 'runner' || m.role === 'bucket_runner';
                                                const bgColor = memberIsRunner ? 'bg-blue-50' : 'bg-indigo-50';
                                                const borderColor = memberIsRunner ? 'border-blue-100' : 'border-indigo-100';
                                                const avatarBg = memberIsRunner ? 'bg-blue-500' : 'bg-indigo-500';
                                                const nameColor = memberIsRunner ? 'text-blue-900' : 'text-indigo-900';
                                                const subColor = memberIsRunner ? 'text-blue-600' : 'text-indigo-600';
                                                const ringColor = memberIsRunner ? 'ring-blue-300' : 'ring-indigo-300';
                                                const roleLabel = memberIsRunner ? 'Bucket Runner' : 'Picker';

                                                return (
                                                    <div
                                                        key={m.id}
                                                        className={`p-2.5 ${bgColor} rounded-xl border ${borderColor} ${onViewPicker ? 'cursor-pointer hover:shadow-sm transition-all' : ''}`}
                                                        onClick={() => onViewPicker && onViewPicker(m)}
                                                    >
                                                        <div className="flex items-center gap-2.5">
                                                            <div
                                                                className={`w-8 h-8 rounded-full ${avatarBg} flex items-center justify-center text-white text-[10px] font-bold flex-shrink-0 ${onViewPicker ? `hover:ring-2 ${ringColor}` : ''}`}
                                                            >
                                                                {m.name.split(' ').map(n => n[0]).join('').slice(0, 2).toUpperCase()}
                                                            </div>
                                                            <div className="flex-1 min-w-0">
                                                                <p className={`text-sm font-bold ${nameColor} truncate`}>{m.name}</p>
                                                                <p className={`text-[10px] ${subColor}`}>
                                                                    {roleLabel} · {m.total_buckets_today || 0} buckets
                                                                </p>
                                                            </div>
                                                            {onViewPicker && (
                                                                <span className="material-symbols-outlined text-slate-300 text-sm">chevron_right</span>
                                                            )}
                                                        </div>
                                                    </div>
                                                );
                                            })}
                                        </div>
                                    )}
                                </div>
                            ))}
                        </div>
                    ) : (
                        <div className="p-3 bg-slate-50 rounded-xl border border-slate-100 text-center">
                            <p className="text-xs text-slate-400">No team assigned to Row {initialRow}</p>
                        </div>
                    )}

                    {/* Row selection grid */}
                    <div>
                        <div className="flex items-center justify-between mb-2">
                            <label className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">
                                {selectedBlock?.name || 'Block'} Rows
                            </label>
                            <span className="text-[10px] font-bold text-primary">{selectedRows.length} selected</span>
                        </div>
                        <div className="grid grid-cols-6 gap-1.5">
                            {blockRows.map(r => {
                                const isSelected = selectedRows.includes(r);
                                const teams = teamsPerRow[r] || [];
                                const hasTeams = teams.length > 0;
                                // Block rows of non-active variety
                                const rowVariety = selectedBlock?.rowVarieties?.[r];
                                const isVarietyBlocked = selectedVariety !== 'ALL' && rowVariety && rowVariety !== selectedVariety;
                                return (
                                    <button
                                        key={r}
                                        onClick={() => !isVarietyBlocked && toggleRow(r)}
                                        disabled={!!isVarietyBlocked}
                                        className={`py-2 rounded-lg text-xs font-bold transition-all relative
                                            ${isVarietyBlocked
                                                ? 'bg-slate-100 text-slate-300 cursor-not-allowed opacity-40'
                                                : isSelected
                                                    ? 'bg-primary text-white shadow-sm scale-105'
                                                    : hasTeams
                                                        ? 'bg-amber-50 text-amber-600 hover:bg-amber-100'
                                                        : 'bg-slate-50 text-slate-500 hover:bg-slate-100'
                                            }`}
                                        title={isVarietyBlocked ? `${rowVariety} (inactive)` : hasTeams ? `${teams.length} team(s), ${teams.reduce((s, t) => s + t.total, 0)} people` : undefined}
                                    >
                                        {r}
                                        {hasTeams && !isSelected && !isVarietyBlocked && (
                                            <span className="absolute -top-0.5 -right-0.5 w-2 h-2 bg-amber-400 rounded-full" />
                                        )}
                                    </button>
                                );
                            })}
                        </div>
                    </div>

                    {/* Team Leader */}
                    <div>
                        <label className="text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-1.5 block">Team Leader</label>
                        <select
                            value={selectedLeader}
                            onChange={(e) => setSelectedLeader(e.target.value)}
                            aria-label="Assign team leader to row"
                            className="w-full bg-slate-50 border border-slate-200 rounded-xl px-3 py-2.5 text-sm font-bold outline-none text-text-main focus:border-primary focus:ring-1 focus:ring-primary/20 transition-all"
                        >
                            <option value="">Select leader...</option>
                            {teamLeaders.map(tl => {
                                const tlRows = getLeaderRows(tl.id);
                                const rowLabel = tlRows.length > 0 ? ` (R${tlRows.join(', R')})` : '';
                                const memberCount = crew.filter(p => p.team_leader_id === tl.id).length;
                                return (
                                    <option key={tl.id} value={tl.id}>{tl.name} · {memberCount}p{rowLabel}</option>
                                );
                            })}
                        </select>

                        {/* Mini team preview */}
                        {selectedLeader && (() => {
                            const members = crew.filter(p => p.team_leader_id === selectedLeader);
                            if (members.length === 0) return null;
                            return (
                                <div className="flex items-center gap-1.5 mt-2 flex-wrap">
                                    {members.slice(0, 6).map(m => {
                                        const isRunnerMember = m.role === 'runner' || m.role === 'bucket_runner';
                                        return (
                                            <div key={m.id} className="flex items-center gap-1">
                                                <div className={`w-5 h-5 rounded-full ${isRunnerMember ? 'bg-blue-500' : 'bg-indigo-500'} flex items-center justify-center text-white text-[7px] font-bold`}>
                                                    {m.name.split(' ').map(n => n[0]).join('').slice(0, 2).toUpperCase()}
                                                </div>
                                                <span className={`text-[10px] font-bold ${isRunnerMember ? 'text-blue-600' : 'text-indigo-600'}`}>{m.name.split(' ')[0]}</span>
                                            </div>
                                        );
                                    })}
                                    {members.length > 6 && <span className="text-[10px] text-slate-400">+{members.length - 6}</span>}
                                </div>
                            );
                        })()}
                    </div>

                    {/* Side Toggle */}
                    <div>
                        <label className="text-[10px] font-bold text-slate-400 uppercase tracking-wider mb-1.5 block">Side</label>
                        <div className="flex bg-slate-100 p-0.5 rounded-xl">
                            {(['north', 'south'] as const).map(side => (
                                <button
                                    key={side}
                                    onClick={() => setSelectedSide(side)}
                                    className={`flex-1 py-2 rounded-lg text-xs font-bold transition-all ${selectedSide === side ? 'bg-white shadow-sm text-primary' : 'text-slate-400'}`}
                                >
                                    {side === 'north' ? 'North' : 'South'}
                                </button>
                            ))}
                        </div>
                    </div>
                </div>

                {/* Confirm */}
                <div className="px-5 pb-5 pt-2">
                    <button
                        onClick={handleAssign}
                        disabled={!selectedLeader || selectedRows.length === 0 || assigning}
                        className="w-full py-3 bg-primary text-white rounded-xl font-bold text-sm disabled:opacity-40 transition-all active:scale-[0.98] hover:shadow-md"
                    >
                        {assigning ? 'Assigning...'
                            : selectedRows.length > 1 ? `Assign ${selectedRows.length} Rows`
                                : 'Confirm Assignment'}
                    </button>
                </div>
            </div>
        </div>
    );
};

export default RowAssignmentModal;
