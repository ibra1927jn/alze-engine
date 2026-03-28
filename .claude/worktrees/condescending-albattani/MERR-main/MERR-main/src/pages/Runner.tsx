// pages/Runner.tsx
import React, { useState, Suspense } from 'react';
import { nowNZST } from '@/utils/nzst';
import BottomNav, { NavTab } from '@/components/common/BottomNav';
import LogisticsView from '../components/views/runner/LogisticsView';
import WarehouseView from '../components/views/runner/WarehouseView';
import MessagingView from '../components/views/runner/MessagingView';
import RunnersView from '../components/views/runner/RunnersView';
import QualityRatingModal from '../components/modals/QualityRatingModal';
import { feedbackService } from '../services/feedback.service';
import { useMessaging } from '@/context/MessagingContext';
import { useHarvestStore } from '@/stores/useHarvestStore';
import { offlineService } from '@/services/offline.service';
import { logger } from '@/utils/logger';
import Toast from '@/components/ui/Toast';
import SyncStatusMonitor from '../components/common/SyncStatusMonitor';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';
import TimesheetEditor from '@/components/views/manager/TimesheetEditor';
import Header from '@/components/common/Header';

// Lazy-load ScannerModal — html5-qrcode is ~250KB, only needed when user taps "Scan"
const ScannerModal = React.lazy(() => import('../components/modals/ScannerModal'));

const Runner = () => {
    const inventory = useHarvestStore((state) => state.inventory);
    const orchard = useHarvestStore((state) => state.orchard);
    const crew = useHarvestStore((state) => state.crew);

    const [selectedBinId, setSelectedBinId] = useState<string | undefined>(undefined);
    const [activeTab, setActiveTab] = useState<'logistics' | 'runners' | 'warehouse' | 'messaging' | 'timesheet'>('logistics');
    const [pendingUploads, setPendingUploads] = useState<number>(0);
    const [showScanner, setShowScanner] = useState<boolean>(false);
    const [scanType, setScanType] = useState<'BIN' | 'BUCKET'>('BUCKET');
    const [toast, setToast] = useState<{ message: string; type: 'success' | 'error' | 'info' | 'warning' } | null>(null);

    const { sendBroadcast } = useMessaging();

    const fetchGlobalData = useHarvestStore((state) => state.fetchGlobalData);
    React.useEffect(() => {
        fetchGlobalData();
    }, [fetchGlobalData]);

    // Poll for pending uploads — 5s interval, pauses when tab is hidden
    React.useEffect(() => {
        const poll = async () => {
            if (document.visibilityState === 'visible') {
                const count = await offlineService.getPendingCount();
                setPendingUploads(count);
            }
        };
        poll(); // Initial check
        const interval = setInterval(poll, 5000);
        return () => clearInterval(interval);
    }, []);

    const handleScanClick = (type: 'BIN' | 'BUCKET' = 'BUCKET') => {
        feedbackService.vibrate(50);
        setScanType(type);
        setShowScanner(true);
    };

    const handleBroadcast = (message: string) => {
        sendBroadcast("Runner Request", message, 'normal');
        feedbackService.vibrate(50);
        setToast({ message: 'Broadcast Sent!', type: 'success' });
    };

    // Quality Assessment State
    const [qualityScan, setQualityScan] = useState<{ code: string; step: 'SCAN' | 'QUALITY' } | null>(null);

    const addBucket = useHarvestStore((state) => state.addBucket);

    const handleScanComplete = (scannedData: string) => {
        // 1. Close Scanner UI immediately
        setShowScanner(false);

        // 2. Validate Data
        if (!scannedData) return;

        if (scanType === 'BIN') {
            const bin = inventory?.find(b => b.bin_code === scannedData || b.id === scannedData);
            if (bin) {
                setSelectedBinId(bin.id);
                feedbackService.vibrate(100);
                setToast({ message: `Bin ${bin.bin_code || 'Selected'} Active`, type: 'info' });
            } else {
                setToast({ message: 'Bin not found in system', type: 'error' });
            }
            return;
        }

        // 3. Validate picker is checked in before accepting bucket
        const isCheckedIn = crew.some(p =>
            p.id === scannedData || p.picker_id === scannedData
        );
        if (!isCheckedIn) {
            setToast({
                message: '⚠️ Picker not checked in. Ask Team Leader to check them in first.',
                type: 'warning'
            });
            return;
        }

        // 4. Open Quality Selection for Buckets
        setQualityScan({ code: scannedData, step: 'QUALITY' });
        feedbackService.vibrate(50);
    };

    const submitQuality = async (grade: 'A' | 'B' | 'C' | 'reject') => {
        if (!qualityScan) return;

        const { code } = qualityScan;
        setQualityScan(null);
        logger.debug(`[Runner] Scanning bucket with bin_id: ${selectedBinId}`);

        addBucket({
            picker_id: code,
            quality_grade: grade,
            timestamp: nowNZST(),
            orchard_id: orchard?.id || 'offline_pending',
        });

        feedbackService.triggerSuccess();
        setToast({ message: 'Bucket Saved (Offline Ready)', type: 'success' });
    };

    // Calculate real inventory data from context
    const displayInventory = React.useMemo(() => {
        const full = (inventory || []).filter(b => b.status === 'full').length;
        const empty = (inventory || []).filter(b => b.status === 'empty').length;
        const inProgress = (inventory || []).filter(b => b.status === 'in-progress').length;

        return {
            full_bins: full,
            empty_bins: empty,
            in_progress: inProgress,
            total: (inventory || []).length || 50,
            raw: inventory || []
        };
    }, [inventory]);

    return (
        <div className="bg-background-light min-h-screen font-display text-text-main flex flex-col relative overflow-hidden pb-20">

            <Header
                title="Runner"
                subtitle={`Logistics - ${orchard?.name || 'No Orchard'}`}
            />

            {/* Global Toast Container */}
            {toast && (
                <Toast
                    message={toast.message}
                    type={toast.type}
                    onClose={() => setToast(null)}
                />
            )}

            {/* Main Content Area */}
            <main className="flex-1 overflow-hidden flex flex-col relative z-0">
                {/* Global Offline Sync Banner */}
                <SyncStatusMonitor />

                <div key={activeTab} className="animate-fade-in flex-1 overflow-hidden flex flex-col">
                    {activeTab === 'logistics' && (
                        <ComponentErrorBoundary componentName="Logistics">
                            <LogisticsView
                                onScan={handleScanClick}
                                pendingUploads={pendingUploads}
                                inventory={displayInventory}
                                onBroadcast={handleBroadcast}
                                selectedBinId={selectedBinId}
                            />
                        </ComponentErrorBoundary>
                    )}
                    {activeTab === 'runners' && <ComponentErrorBoundary componentName="Runners"><RunnersView onBack={() => setActiveTab('logistics')} /></ComponentErrorBoundary>}
                    {activeTab === 'warehouse' && (
                        <ComponentErrorBoundary componentName="Warehouse">
                            <WarehouseView
                                inventory={displayInventory}
                                onTransportRequest={() => handleBroadcast("Warehouse is full. Pickup needed.")}
                            />
                        </ComponentErrorBoundary>
                    )}
                    {activeTab === 'messaging' && <ComponentErrorBoundary componentName="Messaging"><MessagingView /></ComponentErrorBoundary>}
                    {activeTab === 'timesheet' && (
                        <ComponentErrorBoundary componentName="Timesheet">
                            <div className="p-4">
                                <TimesheetEditor orchardId={orchard?.id || ''} />
                            </div>
                        </ComponentErrorBoundary>
                    )}
                </div>
            </main>

            {/* Bottom Navigation */}
            <BottomNav
                tabs={[
                    { id: 'logistics', label: 'Logistics', icon: 'local_shipping' },
                    { id: 'runners', label: 'Runners', icon: 'groups' },
                    { id: 'warehouse', label: 'Warehouse', icon: 'warehouse' },
                    { id: 'timesheet', label: 'Timesheet', icon: 'schedule' },
                    { id: 'messaging', label: 'Chat', icon: 'forum' },
                ] as NavTab[]}
                activeTab={activeTab}
                onTabChange={(id) => setActiveTab(id as typeof activeTab)}
            />

            {/* Modals */}
            {showScanner && (
                <Suspense fallback={<div className="fixed inset-0 z-50 bg-black/60 flex items-center justify-center"><div className="w-10 h-10 border-4 border-white/30 border-t-white rounded-full animate-spin" /></div>}>
                    <ScannerModal
                        onClose={() => setShowScanner(false)}
                        onScan={handleScanComplete}
                        scanType="BUCKET"
                    />
                </Suspense>
            )}
            {/* Quality Modal */}
            {qualityScan?.step === 'QUALITY' && (
                <QualityRatingModal
                    scannedCode={qualityScan.code}
                    onRate={submitQuality}
                    onCancel={() => setQualityScan(null)}
                />
            )}
        </div>
    );
};

export default Runner;
