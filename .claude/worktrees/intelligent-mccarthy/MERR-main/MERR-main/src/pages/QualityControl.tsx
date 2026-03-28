/**
 * QualityControl.tsx — QC Inspector Dashboard
 *
 * Standardized: uses BottomNav (field-mobile role) + ComponentErrorBoundary
 */
import { useState, useEffect, useCallback } from 'react';
import { qcService, QCInspection, GradeDistribution } from '@/services/qc.service';
import { useHarvestStore } from '@/stores/useHarvestStore';
import { useAuth } from '@/context/AuthContext';
import { Picker } from '@/types';
import { logger } from '@/utils/logger';
import BottomNav, { NavTab } from '@/components/common/BottomNav';
import InspectTab from '@/components/views/qc/InspectTab';
import HistoryTab from '@/components/views/qc/HistoryTab';
import StatsTab from '@/components/views/qc/StatsTab';
import TrendsTab from '@/components/views/qc/TrendsTab';
import ComponentErrorBoundary from '@/components/ui/ComponentErrorBoundary';
import LoadingSkeleton from '@/components/ui/LoadingSkeleton';
import Header from '@/components/common/Header';

type QualityGrade = 'A' | 'B' | 'C' | 'reject';

export default function QualityControl() {
    const [activeTab, setActiveTab] = useState<'inspect' | 'history' | 'stats' | 'trends'>('inspect');
    const [selectedPicker, setSelectedPicker] = useState<Picker | null>(null);
    const [notes, setNotes] = useState('');
    const [isSubmitting, setIsSubmitting] = useState(false);
    const [lastGrade, setLastGrade] = useState<{ grade: QualityGrade; picker: string } | null>(null);
    const [isLoading, setIsLoading] = useState(true);

    // Data state
    const [inspections, setInspections] = useState<QCInspection[]>([]);
    const [distribution, setDistribution] = useState<GradeDistribution>({ A: 0, B: 0, C: 0, reject: 0, total: 0 });

    const { crew, orchard } = useHarvestStore();
    const { appUser } = useAuth();
    const orchardId = orchard?.id;

    // Load inspections
    const loadInspections = useCallback(async () => {
        if (!orchardId) { setIsLoading(false); return; }
        setIsLoading(true);
        const data = await qcService.getInspections(orchardId);
        setInspections(data);
        const dist = await qcService.getGradeDistribution(orchardId);
        setDistribution(dist);
        setIsLoading(false);
    }, [orchardId]);

    useEffect(() => {
        loadInspections();
    }, [loadInspections]);

    // Handle grade submission
    const handleGrade = async (grade: QualityGrade, photoBlob?: Blob | null) => {
        if (!selectedPicker || !orchardId || !appUser?.id) return;
        setIsSubmitting(true);

        let photoUrl: string | undefined;

        // Upload photo if provided
        if (photoBlob) {
            try {
                const dateStr = new Date().toISOString().slice(0, 10);
                const fileName = `${orchardId}/${dateStr}/${crypto.randomUUID()}.webp`;
                const { data: uploadData, error: uploadError } = await (await import('@/services/supabase')).supabase.storage
                    .from('qc-photos')
                    .upload(fileName, photoBlob, {
                        contentType: 'image/webp',
                        cacheControl: '31536000',
                    });

                if (uploadError) {
                    logger.error('[QC] Photo upload failed:', uploadError.message);
                } else if (uploadData?.path) {
                    const { data: urlData } = (await import('@/services/supabase')).supabase.storage
                        .from('qc-photos')
                        .getPublicUrl(uploadData.path);
                    photoUrl = urlData.publicUrl;
                }
            } catch (err) {
                logger.error('[QC] Photo upload error:', err);
            }
        }

        const result = await qcService.logInspection({
            orchardId,
            pickerId: selectedPicker.id,
            inspectorId: appUser.id,
            grade,
            notes: notes.trim() || undefined,
            photoUrl,
        });

        if (result) {
            setLastGrade({ grade, picker: selectedPicker.name });
            setNotes('');
            await loadInspections();
            setTimeout(() => setLastGrade(null), 3000);
        }

        setIsSubmitting(false);
    };

    if (isLoading) {
        return (
            <div className="min-h-screen bg-background-light p-6">
                <div className="max-w-2xl mx-auto space-y-4">
                    <LoadingSkeleton type="metric" count={4} />
                    <LoadingSkeleton type="list" count={3} />
                </div>
            </div>
        );
    }

    return (
        <div className="min-h-screen bg-background-light font-display flex flex-col pb-20">
            <Header
                title="Quality Control"
                subtitle="Inspection Dashboard"
            />
            <main className="flex-1 w-full relative">
                <div key={activeTab} className="animate-fade-in">
                    {activeTab === 'inspect' && (
                        <ComponentErrorBoundary componentName="Inspect">
                            <InspectTab
                                crew={crew}
                                distribution={distribution}
                                selectedPicker={selectedPicker}
                                setSelectedPicker={setSelectedPicker}
                                notes={notes}
                                setNotes={setNotes}
                                isSubmitting={isSubmitting}
                                lastGrade={lastGrade}
                                onGrade={handleGrade}
                                onAutoAdvance={() => {
                                    if (selectedPicker && crew.length > 0) {
                                        const currentIndex = crew.findIndex(p => p.id === selectedPicker.id);
                                        const nextIndex = (currentIndex + 1) % crew.length;
                                        setSelectedPicker(crew[nextIndex]);
                                    }
                                }}
                            />
                        </ComponentErrorBoundary>
                    )}
                    {activeTab === 'history' && (
                        <ComponentErrorBoundary componentName="Inspection History">
                            <HistoryTab inspections={inspections} crew={crew} />
                        </ComponentErrorBoundary>
                    )}
                    {activeTab === 'stats' && (
                        <ComponentErrorBoundary componentName="Quality Stats">
                            <StatsTab distribution={distribution} />
                        </ComponentErrorBoundary>
                    )}
                    {activeTab === 'trends' && orchardId && (
                        <ComponentErrorBoundary componentName="Quality Trends">
                            <TrendsTab orchardId={orchardId} />
                        </ComponentErrorBoundary>
                    )}
                </div>
            </main>

            <BottomNav
                tabs={[
                    { id: 'inspect', label: 'Inspect', icon: 'nutrition' },
                    { id: 'history', label: 'History', icon: 'assignment_turned_in' },
                    { id: 'stats', label: 'Analytics', icon: 'bar_chart' },
                    { id: 'trends', label: 'Trends', icon: 'trending_up' },
                ] as NavTab[]}
                activeTab={activeTab}
                onTabChange={(id) => setActiveTab(id as typeof activeTab)}
            />
        </div>
    );
}
