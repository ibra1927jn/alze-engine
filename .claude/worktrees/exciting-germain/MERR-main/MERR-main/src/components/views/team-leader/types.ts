/**
 * UIPicker and related types for Team Leader views
 */

export interface UIPicker {
    id: string;
    name: string;
    avatar: string;
    idNumber: string;
    harnessNumber: string;
    startTime: string;
    assignedRow?: number;
    bucketsToday: number;
    hoursWorked: number;
    hourlyRate: number;
    status: 'Active' | 'Break' | 'Below Minimum' | 'Off Duty';
    earningsToday: number;
    qcStatus: ('excellent' | 'good' | 'warning')[];
}

export interface UIRowAssignment {
    rowNumber: number;
    side: 'North' | 'South';
    assignedPickers: string[];
    completionPercentage: number;
    status: 'Active' | 'Assigned' | 'Completed';
}

export interface DayConfig {
    orchard: string;
    variety: string;
    targetSize: string;
    targetColor: string;
    binType: 'Standard' | 'Export' | 'Process';
}
