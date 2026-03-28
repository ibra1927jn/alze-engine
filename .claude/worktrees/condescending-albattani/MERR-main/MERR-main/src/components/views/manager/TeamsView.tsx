/**
 * components/views/manager/TeamsView.tsx
 * REFACTORED: Uses crew prop from context with manual refresh + unlink support.
 */
import React, { useState, useMemo, useCallback } from 'react';
import { Picker, Role, HarvestSettings } from '../../../types';
import TeamLeaderCard from './TeamLeaderCard';
import TeamLeaderSelectionModal from '../../modals/TeamLeaderSelectionModal';
import ImportCSVModal from '../../modals/ImportCSVModal';
import TeamsToolbar from './teams/TeamsToolbar';
import RunnersSection from './teams/RunnersSection';

interface TeamsViewProps {
    crew: Picker[];
    setShowAddUser?: (show: boolean) => void;
    setSelectedUser: (user: Picker) => void;
    settings: HarvestSettings | null;
    orchardId?: string;
    onRefresh?: () => Promise<void>;
    onRemoveUser?: (userId: string) => Promise<void>;
}

const TeamsView: React.FC<TeamsViewProps> = ({
    crew,
    setSelectedUser,
    settings,
    orchardId,
    onRefresh,
    onRemoveUser
}) => {
    const [search, setSearch] = useState('');
    const [isAddTeamLeaderModalOpen, setIsAddTeamLeaderModalOpen] = useState(false);
    const [showImportCSV, setShowImportCSV] = useState(false);

    const handleImportComplete = useCallback((_count: number) => {
        setShowImportCSV(false);
        onRefresh?.();
    }, [onRefresh]);

    const { leaders, runners, groupedCrew } = useMemo(() => {
        const activeCrew = crew.filter(p => p.status !== 'inactive');
        const leaders = activeCrew.filter(p => p.role === 'team_leader' || p.role === Role.TEAM_LEADER);
        const runners = activeCrew.filter(p => p.role === 'runner' || p.role === Role.RUNNER);
        const grouped: Record<string, Picker[]> = {};

        activeCrew.forEach(p => {
            if (p.team_leader_id && p.role === 'picker') {
                if (!grouped[p.team_leader_id]) grouped[p.team_leader_id] = [];
                grouped[p.team_leader_id].push(p);
            }
        });
        return { leaders, runners, groupedCrew: grouped };
    }, [crew]);

    const filteredLeaders = leaders.filter(l => l.name?.toLowerCase().includes(search.toLowerCase()));
    const filteredRunners = runners.filter(r => r.name?.toLowerCase().includes(search.toLowerCase()));

    return (
        <div className="flex flex-col h-full">
            <TeamsToolbar
                orchardId={orchardId}
                usersCount={crew.length}
                setIsAddTeamLeaderModalOpen={setIsAddTeamLeaderModalOpen}
                setShowImportCSV={setShowImportCSV}
                search={search}
                setSearch={setSearch}
            />

            <div className="flex-1 overflow-y-auto p-4 md:p-6 space-y-8">
                <div className="section-enter stagger-1">
                    <RunnersSection
                        runners={filteredRunners}
                        onSelectUser={setSelectedUser}
                        onRemoveUser={onRemoveUser}
                    />
                </div>

                <section className="glass-card p-5 section-enter stagger-3">
                    <div className="flex items-center justify-between mb-4">
                        <h3 className="text-lg font-black flex items-center gap-2">
                            <span className="material-symbols-outlined text-orange-500">groups</span>
                            Harvest Teams
                        </h3>
                        <span className="text-xs bg-orange-50 text-orange-600 px-3 py-1 rounded-full font-bold">
                            {filteredLeaders.length} {filteredLeaders.length === 1 ? 'leader' : 'leaders'}
                        </span>
                    </div>
                    {filteredLeaders.length > 0 ? (
                        <div className="space-y-4">
                            {filteredLeaders.map((leader, idx) => (
                                <TeamLeaderCard
                                    key={leader.id}
                                    leader={leader}
                                    crew={groupedCrew[leader.id] || []}
                                    onSelectUser={setSelectedUser}
                                    settings={settings}
                                    staggerIndex={idx}
                                    onRemoveUser={onRemoveUser}
                                />
                            ))}
                        </div>
                    ) : (
                        <div className="text-center py-10 italic bg-slate-50 rounded-xl border border-dashed border-border-light text-slate-400 empty-state-enter">
                            <span className="material-symbols-outlined text-3xl block mb-2">group_off</span>
                            <p>No teams found. Assign a leader to start.</p>
                        </div>
                    )}
                </section>
            </div>

            {isAddTeamLeaderModalOpen && (
                <TeamLeaderSelectionModal
                    onClose={() => setIsAddTeamLeaderModalOpen(false)}
                    orchardId={orchardId}
                    onRemoveUser={onRemoveUser}
                    onAdd={async () => {
                        setIsAddTeamLeaderModalOpen(false);
                        // Refresh the crew list after assignment
                        await onRefresh?.();
                    }}
                />
            )}

            <ImportCSVModal
                isOpen={showImportCSV}
                onClose={() => setShowImportCSV(false)}
                orchardId={orchardId || ''}
                existingPickers={crew.map(p => ({ picker_id: p.picker_id, name: p.name }))}
                onImportComplete={handleImportComplete}
            />
        </div>
    );
};

export default React.memo(TeamsView);
