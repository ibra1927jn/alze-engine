/**
 * MANAGER.TSX — Adaptive Command Center
 *
 * Desktop (md+): DesktopLayout sidebar with all navigation items.
 * Mobile: BottomNav with 5 essential tabs + "More" menu for secondary views.
 *
 * Modals extracted to components/manager/modals/
 */
import React, { useState, useMemo, Suspense } from 'react';
import { useHarvestStore as useHarvest } from '@/stores/useHarvestStore';
import { useMessaging } from '@/context/MessagingContext';
import { Role, Tab, Picker } from '@/types';
import BottomNav from '@/components/common/BottomNav';
import DesktopLayout from '@/components/common/DesktopLayout';
import { useMediaQuery } from '@/hooks/useMediaQuery';
import { MOBILE_TABS, DESKTOP_NAV } from './managerNav.config';
import { useOfflineQueue } from '@/hooks/useOfflineQueue';

import { useEffect } from 'react';
import { notificationService } from '@/services/notification.service';
import { userService } from '@/services/user.service';
import { db } from '@/services/db';

// Modular Views — eager (lightweight or always visible)
import DashboardView from '@/components/views/manager/DashboardView';
import TeamsView from '@/components/views/manager/TeamsView';
import MoreMenuView from '@/components/views/manager/MoreMenuView';

// Lazy-loaded views (code-split for mobile performance)
const LogisticsView = React.lazy(() => import('@/components/views/manager/LogisticsView'));
const MessagingView = React.lazy(() => import('@/components/views/manager/MessagingView'));
const MapToggleView = React.lazy(() => import('@/components/views/manager/MapToggleView'));
const InsightsView = React.lazy(() => import('@/components/views/manager/InsightsView'));
const SettingsView = React.lazy(() => import('@/components/views/manager/SettingsView'));
// TimesheetEditor and DeadLetterQueueView available for other roles
// const TimesheetEditor = React.lazy(() => import('@/components/views/manager/TimesheetEditor'));
// const DeadLetterQueueView = React.lazy(() => import('@/components/views/manager/DeadLetterQueueView'));
import { logger } from '@/utils/logger';
import PickerProfileDrawer from '@/components/common/PickerProfileDrawer';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';

// Components
import Header from '@/components/common/Header';

// Modals
import DaySettingsModal from '@/components/modals/DaySettingsModal';
import AddPickerModal from '@/components/modals/AddPickerModal';
import BroadcastModal from '@/components/views/manager/BroadcastModal';
import RowAssignmentModal from '@/components/views/manager/RowAssignmentModal';
import PickerDetailsModal from '@/components/modals/PickerDetailsModal';

/* ── Navigation configs imported from managerNav.config.ts ── */

/* ── Lazy loading fallback ─────────────────────────── */
const TabLoader = () => (
    <div className="flex items-center justify-center py-32">
        <div className="text-center">
            <div className="w-8 h-8 border-3 border-primary border-t-transparent rounded-full animate-spin mx-auto mb-3" />
            <p className="text-sm text-text-sub font-medium">Loading…</p>
        </div>
    </div>
);

const Manager = () => {
    const {
        stats,
        crew = [],
        inventory = [],
        orchard,
        settings,
        updateSettings,
        addPicker,
        removePicker,
        presentCount,
        bucketRecords,
        fetchGlobalData,
        updatePicker,
        assignRow,
    } = useHarvest();

    const { sendBroadcast, sendMessage, getOrCreateConversation } = useMessaging();
    const isDesktop = useMediaQuery('(min-width: 768px)');

    // Trigger data fetch on mount
    useEffect(() => {
        fetchGlobalData();
    }, [fetchGlobalData]);

    // Start/stop notification checking based on user preferences
    useEffect(() => {
        const prefs = notificationService.getPrefs();
        if (prefs.enabled) {
            notificationService.startChecking();
        }
        return () => {
            notificationService.stopChecking();
        };
    }, []);

    // Process queued offline operations (extracted to useOfflineQueue hook)
    useOfflineQueue(fetchGlobalData);

    // Filter bucket records for today (performance optimization)
    const filteredBucketRecords = useMemo(() => {
        if (!bucketRecords) return [];
        const startOfDay = new Date();
        startOfDay.setHours(0, 0, 0, 0);
        return bucketRecords.filter(r => new Date(r.scanned_at || '').getTime() >= startOfDay.getTime());
    }, [bucketRecords]);

    // Tab State
    const [activeTab, setActiveTab] = useState<Tab>('dashboard');

    // Persist selected orchard ID to localStorage
    const [selectedOrchardId, setSelectedOrchardId] = useState<string | undefined>(
        () => localStorage.getItem('active_orchard_id') || undefined
    );

    React.useEffect(() => {
        if (orchard?.id) {
            setSelectedOrchardId(orchard.id);
            localStorage.setItem('active_orchard_id', orchard.id);
        }
    }, [orchard?.id]);

    // Modal States
    const [showSettings, setShowSettings] = useState(false);
    const [showAddUser, setShowAddUser] = useState(false);
    const [showBroadcast, setShowBroadcast] = useState(false);
    const [showAssignment, setShowAssignment] = useState<{ show: boolean, row: number }>({ show: false, row: 1 });
    const [selectedUser, setSelectedUser] = useState<Picker | null>(null);

    // Derived Data
    const activeRunners = crew.filter(p => p.role === 'runner' || p.role === Role.RUNNER);
    const teamLeaders = crew.filter(p => p.role === 'team_leader' || p.role === Role.TEAM_LEADER);
    const fullBins = inventory.filter(b => b.status === 'full').length;
    const emptyBins = inventory.filter(b => b.status === 'empty').length;

    // Broadcast Handler
    const handleBroadcast = async (title: string, message: string, priority: 'normal' | 'high' | 'urgent') => {
        await sendBroadcast?.(title, message, priority);
        setShowBroadcast(false);
    };

    // Direct Message Handler — wires PickerDetailsModal "Message" to real messaging
    const handleSendMessage = async (recipientId: string, message: string) => {
        try {
            const conversationId = await getOrCreateConversation?.(recipientId);
            if (conversationId) {
                await sendMessage?.(conversationId, message, 'normal');
            }
        } catch (err) {
            console.error('[Manager] Failed to send message:', err);
        }
    };

    // Content Renderer
    const renderContent = () => {
        switch (activeTab) {
            case 'dashboard':
                return (
                    <ComponentErrorBoundary componentName="Dashboard">
                        <DashboardView
                            stats={stats}
                            teamLeaders={teamLeaders}
                            crew={crew}
                            presentCount={presentCount}
                            setActiveTab={setActiveTab}
                            bucketRecords={filteredBucketRecords}
                            onUserSelect={(user) => {
                                const fullUser = crew.find(p => p.id === user.id || p.picker_id === user.picker_id) || user as Picker;
                                setSelectedUser(fullUser);
                            }}
                        />
                    </ComponentErrorBoundary>
                );
            case 'teams':
                return (
                    <ComponentErrorBoundary componentName="Teams">
                        <TeamsView
                            crew={crew}
                            setShowAddUser={setShowAddUser}
                            setSelectedUser={setSelectedUser}
                            settings={settings}
                            orchardId={selectedOrchardId || orchard?.id}
                            onRefresh={fetchGlobalData}
                            onRemoveUser={async (userId: string) => {
                                logger.debug('[Teams] onRemoveUser called', { userId });
                                // Optimistic removal: immediately remove from local state
                                useHarvest.setState((state: { crew: typeof crew }) => ({
                                    crew: state.crew.filter(c => c.id !== userId),
                                    lastSyncAt: null,
                                }));

                                if (navigator.onLine) {
                                    // Online: execute immediately
                                    try {
                                        await userService.unassignUserFromOrchard(userId);
                                        logger.debug('[Teams] User unlinked successfully');
                                        await fetchGlobalData();
                                    } catch (e) {
                                        logger.error('[Teams] Failed to unlink user:', e);
                                        await fetchGlobalData(); // Revert on failure
                                    }
                                } else {
                                    // Offline: queue for later execution
                                    logger.info('[Teams] Offline — queuing unlink for', userId);
                                    await db.sync_queue.put({
                                        id: `unlink-${userId}-${Date.now()}`,
                                        type: 'UNLINK',
                                        payload: { userId },
                                        timestamp: Date.now(),
                                        retryCount: 0,
                                    });
                                }
                            }}
                        />
                    </ComponentErrorBoundary>
                );
            case 'logistics':
                return (
                    <ComponentErrorBoundary componentName="Logistics">
                        <Suspense fallback={<TabLoader />}>
                            <LogisticsView
                                fullBins={fullBins}
                                emptyBins={emptyBins}
                                activeRunners={activeRunners}
                                onRequestPickup={() => handleBroadcast(
                                    '🚜 Pickup Requested',
                                    'A logistics pickup has been requested at the loading zone.',
                                    'urgent'
                                )}
                                onRunnerClick={(runner) => {
                                    const fullUser = crew.find(p => p.id === runner.id) || runner as Picker;
                                    setSelectedUser(fullUser);
                                }}
                            />
                        </Suspense>
                    </ComponentErrorBoundary>
                );
            case 'messaging':
                return <ComponentErrorBoundary componentName="Messaging"><Suspense fallback={<TabLoader />}><MessagingView /></Suspense></ComponentErrorBoundary>;
            case 'map':
                return (
                    <ComponentErrorBoundary componentName="Map">
                        <Suspense fallback={<TabLoader />}>
                            <MapToggleView
                                totalRows={orchard?.total_rows || 20}
                                crew={crew}
                                bucketRecords={filteredBucketRecords}
                                blockName={orchard?.name || 'Block A'}
                                targetBucketsPerRow={50}
                                setActiveTab={setActiveTab}
                                onRowClick={(rowNum) => setShowAssignment({ show: true, row: rowNum })}
                            />
                        </Suspense>
                    </ComponentErrorBoundary>
                );
            case 'settings':
                return <ComponentErrorBoundary componentName="Settings"><Suspense fallback={<TabLoader />}><SettingsView /></Suspense></ComponentErrorBoundary>;


            case 'insights':
            case 'analytics':
            case 'reports':
                return <ComponentErrorBoundary componentName="Insights"><Suspense fallback={<TabLoader />}><InsightsView /></Suspense></ComponentErrorBoundary>;
            case 'more':
                return <MoreMenuView onNavigate={(tab) => setActiveTab(tab)} />;
            default:
                return (
                    <ComponentErrorBoundary componentName="Dashboard">
                        <DashboardView stats={stats} teamLeaders={teamLeaders} crew={crew} presentCount={presentCount} setActiveTab={setActiveTab} />
                    </ComponentErrorBoundary>
                );
        }
    };

    /* ── Shared Modals ──────────────────────────────────── */
    const renderModals = () => (
        <>
            {showSettings && (
                <DaySettingsModal
                    onClose={() => setShowSettings(false)}
                    settings={{
                        bucketRate: settings?.piece_rate,
                        targetTons: settings?.target_tons
                    }}
                    onSave={(newSettings) => updateSettings({
                        piece_rate: newSettings.bucketRate,
                        target_tons: newSettings.targetTons
                    })}
                />
            )}
            {showAddUser && (
                <AddPickerModal
                    onClose={() => setShowAddUser(false)}
                    onAdd={addPicker}
                />
            )}
            {showBroadcast && (
                <BroadcastModal
                    onClose={() => setShowBroadcast(false)}
                />
            )}
            {showAssignment.show && (
                <RowAssignmentModal
                    initialRow={showAssignment.row}
                    onClose={() => setShowAssignment({ show: false, row: 1 })}
                    onViewPicker={(picker) => {
                        setShowAssignment({ show: false, row: 1 });
                        setSelectedUser(picker);
                    }}
                />
            )}
            {selectedUser && (
                <PickerDetailsModal
                    picker={selectedUser}
                    onClose={() => setSelectedUser(null)}
                    onDelete={removePicker}
                    onUpdate={updatePicker}
                    allCrew={crew}
                    onSendMessage={handleSendMessage}
                    onAssignRow={assignRow}
                />
            )}
        </>
    );

    /* ── Broadcast FAB ──────────────────────────────────── */
    const renderBroadcastFAB = () => {
        if (activeTab === 'map' || activeTab === 'messaging') return null;
        return (
            <div className="fixed bottom-28 md:bottom-8 right-4 z-40">
                <button
                    onClick={() => setShowBroadcast(true)}
                    className="gradient-primary glow-primary text-white rounded-full h-14 px-6 flex items-center justify-center gap-2 transition-all active:scale-95 hover:scale-105 shadow-2xl dash-fab-pulse"
                >
                    <span className="material-symbols-outlined">campaign</span>
                    <span className="font-bold tracking-wide">Broadcast</span>
                </button>
            </div>
        );
    };

    /* ── Desktop Layout ─────────────────────────────────── */
    if (isDesktop) {
        return (
            <>
                <DesktopLayout
                    navItems={DESKTOP_NAV}
                    activeTab={activeTab}
                    onTabChange={(id) => setActiveTab(id as Tab)}
                    title="Harvest Manager"
                    titleIcon="agriculture"
                    accentColor="#16a34a"
                >
                    <div key={activeTab} className="animate-scale-in">
                        {renderContent()}
                    </div>
                </DesktopLayout>
                {renderModals()}
                {renderBroadcastFAB()}
                <PickerProfileDrawer />
            </>
        );
    }

    /* ── Mobile Layout ──────────────────────────────────── */
    return (
        <div className="flex flex-col h-full bg-background-light min-h-screen text-slate-900 pb-20">
            <Header
                title="Harvest Manager"
                subtitle={`${orchard?.name || 'No Orchard'} • Live`}
                onProfileClick={() => setShowSettings(true)}
                onNavigateToMessaging={() => setActiveTab('messaging' as Tab)}
            />

            {/* Content — animate on tab switch */}
            <main className="flex-1 overflow-y-auto">
                <div key={activeTab} className="animate-scale-in">
                    {renderContent()}
                </div>
            </main>

            {renderModals()}

            {/* Navigation Bar — 5 essential tabs */}
            <BottomNav
                tabs={MOBILE_TABS}
                activeTab={activeTab === 'insights' || activeTab === 'messaging' || activeTab === 'settings'
                    ? 'more'
                    : activeTab}
                onTabChange={(id) => setActiveTab(id as Tab)}
            />

            {renderBroadcastFAB()}
            <PickerProfileDrawer />
        </div>
    );
};

export default Manager;
