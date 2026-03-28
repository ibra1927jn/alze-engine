/**
 * useRowAssignments Hook
 * Custom hook for managing row assignments with computed stats
 */
import { useState, useCallback, useMemo } from 'react';
import { useHarvestStore as useHarvest } from '@/stores/useHarvestStore';

interface UIRowAssignment {
    id: string;
    rowNumber: number;
    side: 'North' | 'South';
    assignedPickers: string[];
    completionPercentage: number;
    status: 'Active' | 'Assigned' | 'Completed';
}

interface UseRowAssignmentsReturn {
    rows: UIRowAssignment[];
    isLoading: boolean;
    error: string | null;
    assignNewRow: (rowNumber: number, side: 'north' | 'south', pickerIds: string[]) => Promise<boolean>;
    updateProgress: (rowId: string, percentage: number) => Promise<boolean>;
    completeRowById: (rowId: string) => Promise<boolean>;
    averageCompletion: number;
    activeRows: number;
    completedRows: number;
}

export const useRowAssignments = (): UseRowAssignmentsReturn => {
    const { rowAssignments, assignRow, updateRowProgress, completeRow } = useHarvest();
    const [isLoading, setIsLoading] = useState(false);
    const [error, setError] = useState<string | null>(null);

    // Map context data to UI format
    const rows = useMemo<UIRowAssignment[]>(() => {
        return rowAssignments.map(r => ({
            id: r.id,
            rowNumber: r.row_number,
            side: r.side === 'north' ? 'North' : 'South',
            assignedPickers: r.assigned_pickers,
            completionPercentage: r.completion_percentage,
            status: r.completion_percentage === 100
                ? 'Completed'
                : r.completion_percentage > 0
                    ? 'Active'
                    : 'Assigned'
        }));
    }, [rowAssignments]);

    // Assign new row
    const assignNewRow = useCallback(async (
        rowNumber: number,
        side: 'north' | 'south',
        pickerIds: string[]
    ): Promise<boolean> => {
        setIsLoading(true);
        setError(null);
        try {
            await assignRow(rowNumber, side, pickerIds);
            return true;
        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to assign row');
            return false;
        } finally {
            setIsLoading(false);
        }
    }, [assignRow]);

    // Update row progress
    const updateProgress = useCallback(async (rowId: string, percentage: number): Promise<boolean> => {
        setIsLoading(true);
        setError(null);
        try {
            await updateRowProgress(rowId, percentage);
            return true;
        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to update progress');
            return false;
        } finally {
            setIsLoading(false);
        }
    }, [updateRowProgress]);

    // Complete row
    const completeRowById = useCallback(async (rowId: string): Promise<boolean> => {
        setIsLoading(true);
        setError(null);
        try {
            await completeRow(rowId);
            return true;
        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to complete row');
            return false;
        } finally {
            setIsLoading(false);
        }
    }, [completeRow]);

    // Computed statistics
    const averageCompletion = useMemo(() => {
        if (rows.length === 0) return 0;
        return Math.round(rows.reduce((sum, r) => sum + r.completionPercentage, 0) / rows.length);
    }, [rows]);

    const activeRows = useMemo(() => rows.filter(r => r.status === 'Active').length, [rows]);
    const completedRows = useMemo(() => rows.filter(r => r.status === 'Completed').length, [rows]);

    return {
        rows,
        isLoading,
        error,
        assignNewRow,
        updateProgress,
        completeRowById,
        averageCompletion,
        activeRows,
        completedRows
    };
};

export default useRowAssignments;
