// =============================================
// USE COMPLIANCE HOOK - React hook for compliance monitoring
// =============================================
import { useState, useEffect, useCallback, useMemo } from 'react';
import {
    ComplianceStatus,
    checkPickerCompliance,
} from '../services/compliance.service';
import { Picker } from '../types';
import { nowNZST } from '@/utils/nzst';

export interface PickerComplianceData {
    pickerId: string;
    bucketCount: number;
    workStartTime: Date;
    lastRestBreakAt: Date | null;
    lastMealBreakAt: Date | null;
    lastHydrationAt: Date | null;
}

export interface UseComplianceResult {
    complianceStatuses: Map<string, ComplianceStatus>;
    activeViolations: Array<{
        pickerId: string;
        pickerName?: string;
        violation: ComplianceStatus['violations'][0];
    }>;
    checkPicker: (data: PickerComplianceData, hoursWorked: number) => ComplianceStatus;
    updatePickerData: (pickerId: string, data: Partial<PickerComplianceData>) => void;
    recordBreak: (pickerId: string, breakType: 'rest' | 'meal' | 'hydration') => void;
    getPickerStatus: (pickerId: string) => ComplianceStatus | undefined;
    hasHighSeverityViolations: boolean;
    refreshInterval: number;
}

/**
 * Hook for monitoring NZ compliance across all pickers
 */
export function useCompliance(
    pickers: Picker[],
    workStartTime: Date = new Date(nowNZST()) // 🔧 L36: Use NZST, not UTC
): UseComplianceResult {
    // Store picker compliance data
    const [pickerData, setPickerData] = useState<Map<string, PickerComplianceData>>(new Map());

    // Store computed compliance statuses
    const [complianceStatuses, setComplianceStatuses] = useState<Map<string, ComplianceStatus>>(
        new Map()
    );

    // Initialize picker data when pickers change
    useEffect(() => {
        setPickerData((prev) => {
            const newData = new Map(prev);

            pickers.forEach((picker) => {
                if (!newData.has(picker.id)) {
                    newData.set(picker.id, {
                        pickerId: picker.id,
                        bucketCount: picker.total_buckets_today || 0,
                        workStartTime,
                        lastRestBreakAt: null,
                        lastMealBreakAt: null,
                        lastHydrationAt: null,
                    });
                } else {
                    // Update bucket count
                    const existing = newData.get(picker.id)!;
                    newData.set(picker.id, {
                        ...existing,
                        bucketCount: picker.total_buckets_today || 0,
                    });
                }
            });

            return newData;
        });
    }, [pickers, workStartTime]);

    // Check compliance for a single picker
    const checkPicker = useCallback(
        (data: PickerComplianceData, hoursWorked: number): ComplianceStatus => {
            const now = new Date(nowNZST()); // 🔧 L37: Use NZST for compliance timing
            const minutesWorked = (now.getTime() - data.workStartTime.getTime()) / 60000;

            return checkPickerCompliance({
                pickerId: data.pickerId,
                bucketCount: data.bucketCount,
                hoursWorked,
                consecutiveMinutesWorked: minutesWorked,
                totalMinutesToday: minutesWorked,
                lastRestBreakAt: data.lastRestBreakAt,
                lastMealBreakAt: data.lastMealBreakAt,
                lastHydrationAt: data.lastHydrationAt,
                workStartTime: data.workStartTime,
            });
        },
        []
    );

    // Update picker data
    const updatePickerData = useCallback((pickerId: string, updates: Partial<PickerComplianceData>) => {
        setPickerData((prev) => {
            const newData = new Map(prev);
            const existing = newData.get(pickerId);
            if (existing) {
                newData.set(pickerId, { ...existing, ...updates });
            }
            return newData;
        });
    }, []);

    // Record a break for a picker
    const recordBreak = useCallback((pickerId: string, breakType: 'rest' | 'meal' | 'hydration') => {
        const now = new Date();
        setPickerData((prev) => {
            const newData = new Map(prev);
            const existing = newData.get(pickerId);
            if (existing) {
                const updates: Partial<PickerComplianceData> = {};
                if (breakType === 'rest') updates.lastRestBreakAt = now;
                if (breakType === 'meal') updates.lastMealBreakAt = now;
                if (breakType === 'hydration') updates.lastHydrationAt = now;
                newData.set(pickerId, { ...existing, ...updates });
            }
            return newData;
        });
    }, []);

    // Get status for a specific picker
    const getPickerStatus = useCallback(
        (pickerId: string): ComplianceStatus | undefined => {
            return complianceStatuses.get(pickerId);
        },
        [complianceStatuses]
    );

    // Refresh compliance statuses periodically
    useEffect(() => {
        const refreshStatuses = () => {
            const newStatuses = new Map<string, ComplianceStatus>();
            const now = new Date(nowNZST()); // 🔧 L37: Use NZST for compliance timing

            pickerData.forEach((data, pickerId) => {
                const picker = pickers.find((p) => p.id === pickerId);
                const hoursWorked = picker?.hours || (now.getTime() - data.workStartTime.getTime()) / 3600000;

                const status = checkPicker(data, hoursWorked);
                newStatuses.set(pickerId, status);
            });

            setComplianceStatuses(newStatuses);
        };

        // Initial check
        refreshStatuses();

        // Set up interval (check every minute)
        const interval = setInterval(refreshStatuses, 60000);

        return () => clearInterval(interval);
    }, [pickerData, pickers, checkPicker]);

    // Collect all active violations
    const activeViolations = useMemo(() => {
        const violations: UseComplianceResult['activeViolations'] = [];

        complianceStatuses.forEach((status, pickerId) => {
            const picker = pickers.find((p) => p.id === pickerId);

            status.violations.forEach((violation) => {
                violations.push({
                    pickerId,
                    pickerName: picker?.name,
                    violation,
                });
            });
        });

        // Sort by severity (high first)
        return violations.sort((a, b) => {
            const severityOrder = { high: 0, medium: 1, low: 2 };
            return severityOrder[a.violation.severity] - severityOrder[b.violation.severity];
        });
    }, [complianceStatuses, pickers]);

    // Check if there are high severity violations
    const hasHighSeverityViolations = useMemo(() => {
        return activeViolations.some((v) => v.violation.severity === 'high');
    }, [activeViolations]);

    return {
        complianceStatuses,
        activeViolations,
        checkPicker,
        updatePickerData,
        recordBreak,
        getPickerStatus,
        hasHighSeverityViolations,
        refreshInterval: 60000, // 1 minute
    };
}

export default useCompliance;
