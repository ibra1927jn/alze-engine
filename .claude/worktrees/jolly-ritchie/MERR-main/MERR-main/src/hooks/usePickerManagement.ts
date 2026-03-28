/**
 * usePickerManagement Hook
 * Custom hook for managing pickers with loading states and error handling
 */
import { useState, useCallback, useMemo } from 'react';
import { useHarvestStore as useHarvest } from '@/stores/useHarvestStore';
import { Picker, MINIMUM_WAGE, PIECE_RATE, DEFAULT_START_TIME } from '../types';
import { nowNZST } from '@/utils/nzst';

interface PickerWithCalculations extends Picker {
    hoursWorked: number;
    earningsToday: number;
    bucketsPerHour: number;
    displayStatus: 'Active' | 'Break' | 'Below Minimum' | 'Off Duty';
}

interface UsePickerManagementReturn {
    pickers: PickerWithCalculations[];
    isLoading: boolean;
    error: string | null;
    addNewPicker: (pickerData: Omit<Picker, 'id'>) => Promise<boolean>;
    updatePickerData: (id: string, updates: Partial<Picker>) => Promise<boolean>;
    deletePickerById: (id: string) => Promise<boolean>;
    getPickerById: (id: string) => PickerWithCalculations | undefined;
    totalBuckets: number;
    totalEarnings: number;
    activeCount: number;
    belowMinimumPickers: PickerWithCalculations[];
}

const MIN_BUCKETS_PER_HOUR = MINIMUM_WAGE / PIECE_RATE;

const calculateHoursWorked = (startTime: string = DEFAULT_START_TIME): number => {
    // 🔧 L34: Use NZST instead of UTC new Date()
    const now = new Date(nowNZST());
    const [hours, minutes] = startTime.split(':').map(Number);
    const start = new Date();
    start.setHours(hours, minutes, 0, 0);
    if (start > now) return 0;
    const diffMs = now.getTime() - start.getTime();
    return Math.max(0, Math.round((diffMs / (1000 * 60 * 60)) * 10) / 10);
};

const getDisplayStatus = (
    buckets: number,
    hoursWorked: number,
    baseStatus: string
): 'Active' | 'Break' | 'Below Minimum' | 'Off Duty' => {
    if (baseStatus === 'on_break') return 'Break';
    if (baseStatus === 'inactive' || baseStatus === 'suspended') return 'Off Duty';
    if (hoursWorked > 0 && (buckets / hoursWorked) < MIN_BUCKETS_PER_HOUR) return 'Below Minimum';
    return 'Active';
};

export const usePickerManagement = (): UsePickerManagementReturn => {
    const { crew, addPicker, updatePicker, removePicker } = useHarvest();
    const [isLoading, setIsLoading] = useState(false);
    const [error, setError] = useState<string | null>(null);

    // Calculate derived picker data
    const pickers = useMemo<PickerWithCalculations[]>(() => {
        return crew.map(p => {
            const hoursWorked = calculateHoursWorked(DEFAULT_START_TIME);
            const bucketsPerHour = hoursWorked > 0 ? p.total_buckets_today / hoursWorked : 0;
            // 🔧 L35: Include min wage top-up, not just piece rate
            const pieceEarnings = p.total_buckets_today * PIECE_RATE;
            const minGuarantee = hoursWorked * MINIMUM_WAGE;
            const earningsWithTopUp = pieceEarnings + Math.max(0, minGuarantee - pieceEarnings);
            return {
                ...p,
                hoursWorked,
                earningsToday: earningsWithTopUp,
                bucketsPerHour: Math.round(bucketsPerHour * 10) / 10,
                displayStatus: getDisplayStatus(p.total_buckets_today, hoursWorked, p.status)
            };
        });
    }, [crew]);

    // Add new picker with error handling
    const addNewPicker = useCallback(async (pickerData: Omit<Picker, 'id'>): Promise<boolean> => {
        setIsLoading(true);
        setError(null);
        try {
            await addPicker(pickerData);
            return true;
        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to add picker');
            return false;
        } finally {
            setIsLoading(false);
        }
    }, [addPicker]);

    // Update picker with error handling
    const updatePickerData = useCallback(async (id: string, updates: Partial<Picker>): Promise<boolean> => {
        setIsLoading(true);
        setError(null);
        try {
            await updatePicker(id, updates);
            return true;
        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to update picker');
            return false;
        } finally {
            setIsLoading(false);
        }
    }, [updatePicker]);

    // Delete picker with error handling
    const deletePickerById = useCallback(async (id: string): Promise<boolean> => {
        setIsLoading(true);
        setError(null);
        try {
            await removePicker(id);
            return true;
        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to remove picker');
            return false;
        } finally {
            setIsLoading(false);
        }
    }, [removePicker]);

    // Get single picker by ID
    const getPickerById = useCallback((id: string): PickerWithCalculations | undefined => {
        return pickers.find(p => p.id === id);
    }, [pickers]);

    // Computed statistics
    const totalBuckets = useMemo(() => pickers.reduce((sum, p) => sum + p.total_buckets_today, 0), [pickers]);
    const totalEarnings = useMemo(() => pickers.reduce((sum, p) => sum + p.earningsToday, 0), [pickers]);
    const activeCount = useMemo(() => pickers.filter(p => p.displayStatus !== 'Off Duty').length, [pickers]);
    const belowMinimumPickers = useMemo(() => pickers.filter(p => p.displayStatus === 'Below Minimum'), [pickers]);

    return {
        pickers,
        isLoading,
        error,
        addNewPicker,
        updatePickerData,
        deletePickerById,
        getPickerById,
        totalBuckets,
        totalEarnings,
        activeCount,
        belowMinimumPickers
    };
};

export default usePickerManagement;
