/**
 * usePickerStatus Hook
 * Calculate picker status based on buckets and work hours
 */
import { useMemo } from 'react';

const MIN_WAGE = 23.5;
const PIECE_RATE = 6.5;
const MIN_BUCKETS_PER_HOUR = MIN_WAGE / PIECE_RATE;
const DEFAULT_START_TIME = '07:00';

export type PickerStatusType = 'Active' | 'Break' | 'Below Minimum' | 'Off Duty';

interface UsePickerStatusOptions {
    buckets: number;
    startTime?: string;
    baseStatus?: string;
}

interface UsePickerStatusResult {
    status: PickerStatusType;
    hoursWorked: number;
    bucketsPerHour: number;
    hourlyRate: number;
    isBelowMinimum: boolean;
    earnings: number;
}

export const usePickerStatus = ({
    buckets,
    startTime = DEFAULT_START_TIME,
    baseStatus = 'active',
}: UsePickerStatusOptions): UsePickerStatusResult => {
    return useMemo(() => {
        // Calculate hours worked
        const [startHour, startMin] = startTime.split(':').map(Number);
        const now = new Date();
        const totalMinutes = now.getHours() * 60 + now.getMinutes() - (startHour * 60 + startMin);
        const hoursWorked = Math.max(0.1, totalMinutes / 60);

        // Calculate rates
        const bucketsPerHour = buckets / hoursWorked;
        const earnings = buckets * PIECE_RATE;
        const hourlyRate = earnings / hoursWorked;
        const isBelowMinimum = bucketsPerHour < MIN_BUCKETS_PER_HOUR && hoursWorked >= 1;

        // Determine status
        let status: PickerStatusType = 'Active';
        if (baseStatus === 'on_break') {
            status = 'Break';
        } else if (baseStatus === 'inactive' || baseStatus === 'off_duty') {
            status = 'Off Duty';
        } else if (isBelowMinimum) {
            status = 'Below Minimum';
        }

        return {
            status,
            hoursWorked: Math.round(hoursWorked * 10) / 10,
            bucketsPerHour: Math.round(bucketsPerHour * 10) / 10,
            hourlyRate: Math.round(hourlyRate * 100) / 100,
            isBelowMinimum,
            earnings: Math.round(earnings * 100) / 100,
        };
    }, [buckets, startTime, baseStatus]);
};

export default usePickerStatus;
