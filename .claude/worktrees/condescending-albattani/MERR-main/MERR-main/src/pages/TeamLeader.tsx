import React, { useState, useEffect } from 'react';
import { useHarvestStore } from '@/stores/useHarvestStore';
import BottomNav, { NavTab } from '@/components/common/BottomNav';
import Header from '@/components/common/Header';
import HomeView from '../components/views/team-leader/HomeView';
import TeamView from '../components/views/team-leader/TeamView';
import TasksView from '../components/views/team-leader/TasksView';
import ProfileView from '../components/views/team-leader/ProfileView';
import MessagingView from '../components/views/team-leader/MessagingView';
import AttendanceView from '../components/views/team-leader/AttendanceView';
import TimesheetEditor from '@/components/views/manager/TimesheetEditor';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';

const TeamLeader = () => {
    const fetchGlobalData = useHarvestStore((state) => state.fetchGlobalData);
    const orchard = useHarvestStore((state) => state.orchard);

    useEffect(() => {
        fetchGlobalData();
    }, [fetchGlobalData]);

    // Only these tabs:
    const [activeTab, setActiveTab] = useState<'home' | 'team' | 'tasks' | 'profile' | 'chat' | 'attendance' | 'timesheet'>('home');

    return (
        <div className="bg-background-light font-display min-h-screen flex flex-col pb-20">
            <Header
                title="Team Leader"
                subtitle={`Crew Management`}
                onProfileClick={() => setActiveTab('profile')}
            />
            {/* Full-width layout */}
            <main className="flex-1 w-full relative bg-white shadow-sm">
                <div key={activeTab} className="animate-fade-in">
                    {activeTab === 'home' && <ComponentErrorBoundary componentName="Home"><HomeView onNavigate={(tab) => setActiveTab(tab as typeof activeTab)} /></ComponentErrorBoundary>}
                    {activeTab === 'attendance' && <ComponentErrorBoundary componentName="Attendance"><AttendanceView /></ComponentErrorBoundary>}
                    {activeTab === 'team' && <ComponentErrorBoundary componentName="Team"><TeamView /></ComponentErrorBoundary>}
                    {activeTab === 'tasks' && <ComponentErrorBoundary componentName="Tasks"><TasksView /></ComponentErrorBoundary>}
                    {activeTab === 'timesheet' && (
                        <ComponentErrorBoundary componentName="Timesheet">
                            <div className="p-4">
                                <TimesheetEditor orchardId={orchard?.id || ''} />
                            </div>
                        </ComponentErrorBoundary>
                    )}
                    {activeTab === 'profile' && <ComponentErrorBoundary componentName="Profile"><ProfileView /></ComponentErrorBoundary>}
                    {activeTab === 'chat' && <ComponentErrorBoundary componentName="Chat"><MessagingView /></ComponentErrorBoundary>}
                </div>
            </main>

            {/* Unified Bottom Navigation */}
            <BottomNav
                tabs={[
                    { id: 'home', label: 'Home', icon: 'home' },
                    { id: 'attendance', label: 'Roll Call', icon: 'fact_check' },
                    { id: 'team', label: 'Team', icon: 'groups' },
                    { id: 'timesheet', label: 'Timesheet', icon: 'schedule' },
                    { id: 'chat', label: 'Chat', icon: 'forum' },
                ] as NavTab[]}
                activeTab={activeTab === 'tasks' || activeTab === 'profile' ? 'home' : activeTab}
                onTabChange={(id) => setActiveTab(id as typeof activeTab)}
            />
        </div>
    );
};

export default TeamLeader;