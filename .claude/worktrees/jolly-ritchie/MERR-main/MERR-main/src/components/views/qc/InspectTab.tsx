/**
 * InspectTab — QC Inspection entry tab
 * Extracted from QualityControl.tsx monolith
 *
 * Contains: Picker search, grade buttons, notes input, today's distribution
 */
import React, { useMemo } from 'react';
import { GradeDistribution } from '@/services/qc.service';
import { Picker } from '@/types';
import DistributionBar from './DistributionBar';
import { usePhotoCapture } from '@/hooks/usePhotoCapture';

type QualityGrade = 'A' | 'B' | 'C' | 'reject';

interface GradeConfig {
    label: string;
    sublabel: string;
    color: string;
    bg: string;
    border: string;
    icon: React.ReactNode;
}

const GRADE_CONFIG: Record<QualityGrade, GradeConfig> = {
    A: {
        label: 'Grade A', sublabel: 'Export',
        color: 'text-green-700', bg: 'bg-white hover:bg-green-50',
        border: 'border-l-4 border-l-green-500',
        icon: <span className="material-symbols-outlined text-xl text-green-600">check_circle</span>,
    },
    B: {
        label: 'Grade B', sublabel: 'Domestic',
        color: 'text-blue-700', bg: 'bg-white hover:bg-blue-50',
        border: 'border-l-4 border-l-blue-500',
        icon: <span className="material-symbols-outlined text-xl text-blue-600">check_circle</span>,
    },
    C: {
        label: 'Grade C', sublabel: 'Process',
        color: 'text-amber-700', bg: 'bg-white hover:bg-amber-50',
        border: 'border-l-4 border-l-amber-500',
        icon: <span className="material-symbols-outlined text-xl text-amber-600">warning</span>,
    },
    reject: {
        label: 'Reject', sublabel: 'Discard',
        color: 'text-red-700', bg: 'bg-white hover:bg-red-50',
        border: 'border-l-4 border-l-red-500',
        icon: <span className="material-symbols-outlined text-xl text-red-600">cancel</span>,
    },
};

interface InspectTabProps {
    crew: Picker[];
    distribution: GradeDistribution;
    selectedPicker: Picker | null;
    setSelectedPicker: (p: Picker | null) => void;
    notes: string;
    setNotes: (n: string) => void;
    isSubmitting: boolean;
    lastGrade: { grade: QualityGrade; picker: string } | null;
    onGrade: (grade: QualityGrade, photoBlob?: Blob | null) => void;
    /** If provided, called after a successful grade to auto-select next */
    onAutoAdvance?: () => void;
}

export default function InspectTab({
    crew, distribution, selectedPicker, setSelectedPicker,
    notes, setNotes, isSubmitting, lastGrade, onGrade, onAutoAdvance,
}: InspectTabProps) {
    const [searchQuery, setSearchQuery] = React.useState('');
    const [turboMode, setTurboMode] = React.useState(false);
    const { capturePhoto, photoBlob, photoPreview, isCapturing, clearPhoto } = usePhotoCapture();

    const filteredPickers = useMemo(() => {
        if (!searchQuery.trim()) return crew.slice(0, 5);
        const q = searchQuery.toLowerCase();
        return crew.filter(p =>
            p.name.toLowerCase().includes(q) ||
            p.picker_id.toLowerCase().includes(q)
        );
    }, [crew, searchQuery]);

    // Turbo mode: wrap onGrade to add haptic + auto-advance
    const handleTurboGrade = (grade: QualityGrade) => {
        // Haptic feedback
        if (turboMode && navigator.vibrate) {
            navigator.vibrate(50); // short pulse
        }
        onGrade(grade, photoBlob);
        clearPhoto();
        // Auto-advance will be triggered after the grade completes
        if (turboMode && onAutoAdvance) {
            // Small delay so the success toast renders before advancing
            setTimeout(() => onAutoAdvance(), 300);
        }
    };

    return (
        <div className="space-y-4">
            {/* Turbo Mode Toggle */}
            <div className="flex items-center justify-between bg-gradient-to-r from-amber-50 to-orange-50 border border-amber-200 rounded-lg px-4 py-3">
                <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-amber-600">bolt</span>
                    <div>
                        <p className="text-sm font-semibold text-amber-900">Turbo Mode</p>
                        <p className="text-xs text-amber-600">Auto-advance + haptic + large buttons</p>
                    </div>
                </div>
                <button
                    onClick={() => setTurboMode(!turboMode)}
                    className={`relative inline-flex h-6 w-11 items-center rounded-full transition-colors ${turboMode ? 'bg-amber-500' : 'bg-slate-300'}`}
                    role="switch"
                    aria-checked={turboMode ? "true" : "false"}
                    aria-label="Toggle turbo mode"
                >
                    <span className={`inline-block h-4 w-4 transform rounded-full bg-white transition-transform shadow-sm ${turboMode ? 'translate-x-6' : 'translate-x-1'}`} />
                </button>
            </div>
            {/* Success Toast */}
            {lastGrade && (
                <div className="bg-green-50 border border-green-200 rounded-lg p-3 flex items-center gap-2">
                    <span className="material-symbols-outlined text-base text-green-600">check_circle</span>
                    <span className="text-sm text-green-800">
                        Logged <strong>Grade {lastGrade.grade}</strong> for {lastGrade.picker}
                    </span>
                </div>
            )}

            {/* Step 1: Picker Selection */}
            <div className="bg-white rounded-lg border border-border-light shadow-sm">
                <div className="px-4 py-3 border-b border-slate-100">
                    <h2 className="text-sm font-semibold text-text-main">Select Picker</h2>
                </div>
                <div className="p-4 space-y-3">
                    <div className="relative">
                        <span className="material-symbols-outlined text-base text-text-muted absolute left-3 top-1/2 -translate-y-1/2">search</span>
                        <input
                            type="text"
                            placeholder="Search by name or ID..."
                            value={searchQuery}
                            onChange={(e) => setSearchQuery(e.target.value)}
                            aria-label="Search pickers"
                            className="w-full pl-9 pr-3 py-2.5 border border-border-light rounded-lg text-sm focus:ring-2 focus:ring-indigo-500 focus:border-indigo-500"
                        />
                    </div>

                    <div className="space-y-1 max-h-40 overflow-y-auto">
                        {filteredPickers.map(picker => (
                            <button
                                key={picker.id}
                                onClick={() => setSelectedPicker(picker)}
                                className={`w-full flex items-center gap-3 px-3 py-2.5 rounded-lg text-left transition-colors ${selectedPicker?.id === picker.id
                                    ? 'bg-indigo-50 border border-indigo-200'
                                    : 'hover:bg-slate-50'
                                    }`}
                            >
                                <div className="w-8 h-8 bg-slate-100 rounded-full flex items-center justify-center text-xs font-medium text-text-sub">
                                    {picker.name.split(' ').map(n => n[0]).join('')}
                                </div>
                                <div className="flex-1 min-w-0">
                                    <p className="text-sm font-medium text-text-main truncate">{picker.name}</p>
                                    <p className="text-xs text-text-muted">Row {picker.current_row} · {picker.total_buckets_today} buckets</p>
                                </div>
                                {selectedPicker?.id === picker.id && (
                                    <span className="material-symbols-outlined text-base text-indigo-600">check_circle</span>
                                )}
                            </button>
                        ))}
                        {filteredPickers.length === 0 && (
                            <p className="text-sm text-text-muted text-center py-4">No pickers found</p>
                        )}
                    </div>
                </div>
            </div>

            {/* Step 2: Grade Entry */}
            <div className="bg-white rounded-lg border border-border-light shadow-sm">
                <div className="px-4 py-3 border-b border-slate-100">
                    <h2 className="text-sm font-semibold text-text-main">
                        Assign Grade
                        {selectedPicker && (
                            <span className="ml-2 text-xs text-text-muted font-normal">
                                for {selectedPicker.name}
                            </span>
                        )}
                    </h2>
                </div>
                <div className="p-4">
                    <div className={turboMode ? 'grid grid-cols-1 gap-3' : 'grid grid-cols-2 gap-3'}>
                        {(Object.entries(GRADE_CONFIG) as [QualityGrade, GradeConfig][]).map(
                            ([grade, config]) => (
                                <button
                                    key={grade}
                                    onClick={() => handleTurboGrade(grade)}
                                    disabled={!selectedPicker || isSubmitting}
                                    className={`${config.bg} ${config.border} rounded-lg ${turboMode ? 'p-5' : 'p-4'} flex items-center gap-3 
                                        transition-all border border-border-light shadow-sm
                                        ${!selectedPicker ? 'opacity-40 cursor-not-allowed' : 'active:scale-95'}
                                        ${isSubmitting ? 'pointer-events-none' : ''}`}
                                >
                                    <span className={turboMode ? 'scale-125' : ''}>{config.icon}</span>
                                    <div className="text-left flex-1">
                                        <div className={`font-semibold ${turboMode ? 'text-base' : 'text-sm'} ${config.color}`}>
                                            {config.label}
                                        </div>
                                        <div className={`${turboMode ? 'text-sm' : 'text-xs'} text-text-muted`}>{config.sublabel}</div>
                                        <div className="text-xs text-text-muted mt-0.5">
                                            {distribution[grade as keyof Omit<GradeDistribution, 'total'>]} today
                                        </div>
                                    </div>
                                    {turboMode && (
                                        <span className="material-symbols-outlined text-3xl text-slate-300">touch_app</span>
                                    )}
                                </button>
                            )
                        )}
                    </div>
                </div>
            </div>

            {/* Step 3: Notes (optional) */}
            <div className="bg-white rounded-lg border border-border-light shadow-sm">
                <div className="px-4 py-3 border-b border-slate-100">
                    <h2 className="text-sm font-semibold text-text-main">Add Details (optional)</h2>
                </div>
                <div className="p-4 space-y-3">
                    <textarea
                        value={notes}
                        onChange={(e) => setNotes(e.target.value)}
                        placeholder="Inspection notes..."
                        aria-label="Inspection notes"
                        rows={2}
                        className="w-full border border-border-light rounded-lg px-3 py-2 text-sm resize-none focus:ring-2 focus:ring-indigo-500 focus:border-indigo-500"
                    />
                    <button
                        type="button"
                        onClick={capturePhoto}
                        disabled={isCapturing}
                        className="flex items-center gap-2 px-3 py-2 text-sm text-indigo-600 border border-indigo-200 rounded-lg hover:bg-indigo-50 transition-colors disabled:opacity-50"
                    >
                        <span className="material-symbols-outlined text-base">
                            {isCapturing ? 'hourglass_empty' : 'photo_camera'}
                        </span>
                        {isCapturing ? 'Processing...' : 'Attach Photo'}
                    </button>
                    {photoPreview && (
                        <div className="relative inline-block">
                            <img
                                src={photoPreview}
                                alt="QC inspection photo"
                                className="w-24 h-24 object-cover rounded-lg border border-border-light shadow-sm"
                            />
                            <button
                                type="button"
                                onClick={clearPhoto}
                                className="absolute -top-2 -right-2 w-6 h-6 bg-red-500 text-white rounded-full flex items-center justify-center shadow-sm hover:bg-red-600 transition-colors"
                                aria-label="Remove photo"
                            >
                                <span className="material-symbols-outlined text-sm">close</span>
                            </button>
                        </div>
                    )}
                </div>
            </div>

            {/* Today's Stats */}
            {distribution.total > 0 && (
                <div className="bg-white rounded-lg border border-border-light shadow-sm p-4">
                    <h3 className="text-sm font-semibold text-text-main mb-3">Today&apos;s Distribution</h3>
                    <DistributionBar distribution={distribution} />
                    <div className="flex justify-between mt-2 text-xs text-text-muted">
                        <span>A: {distribution.A} ({distribution.total ? Math.round(distribution.A / distribution.total * 100) : 0}%)</span>
                        <span>B: {distribution.B} ({distribution.total ? Math.round(distribution.B / distribution.total * 100) : 0}%)</span>
                        <span>C: {distribution.C} ({distribution.total ? Math.round(distribution.C / distribution.total * 100) : 0}%)</span>
                        <span>Rej: {distribution.reject} ({distribution.total ? Math.round(distribution.reject / distribution.total * 100) : 0}%)</span>
                    </div>
                </div>
            )}
        </div>
    );
}
