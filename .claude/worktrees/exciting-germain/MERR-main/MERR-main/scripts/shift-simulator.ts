/* eslint-disable no-console */
/**
 * shift-simulator.ts
 * Generates realistic picker activity and bucket scans for testing
 * Injects data into the store WITHOUT persisting to database
 */

import { Picker } from '../src/types';
import { ComplianceViolation } from '../src/services/compliance.service';

export interface SimulationPicker extends Picker {
    bucketsToday: number;
    hoursWorked: number;
    lastBreakTime: Date | null;
    lastMealTime: Date | null;
}

export interface SimulationResult {
    scenario: string;
    timestamp: Date;
    duration: number; // ms
    pickers: SimulationPicker[];
    violations: ComplianceViolation[];
    payroll: {
        totalPiece: number;
        totalMinimum: number;
        finalTotal: number;
    };
    success: boolean;
    errors: string[];
}

/**
 * Generate mock picker data
 */
export function generateMockPickers(count: number, orchardId: string): SimulationPicker[] {
    const names = ['Ana M.', 'Carlos R.', 'Diego F.', 'Elena S.', 'Fernando P.', 'Gabriela L.'];
    const pickers: SimulationPicker[] = [];

    for (let i = 0; i < count; i++) {
        pickers.push({
            id: `sim-picker-${i + 1}`,
            picker_id: `${400 + i}`,
            name: names[i % names.length],
            avatar: names[i % names.length].substring(0, 2),
            current_row: Math.floor(Math.random() * 20) + 1,
            total_buckets_today: 0,
            hours: 0,
            status: 'active',
            safety_verified: true,
            qcStatus: [1, 1, 1],
            orchard_id: orchardId,
            bucketsToday: 0,
            hoursWorked: 0,
            lastBreakTime: null,
            lastMealTime: null
        });
    }

    return pickers;
}

/**
 * Simulate bucket scans over time
 */
export function simulateBucketScans(
    picker: SimulationPicker,
    bucketsPerHour: number,
    hours: number
): void {
    picker.bucketsToday = Math.round(bucketsPerHour * hours);
    picker.total_buckets_today = picker.bucketsToday;
    picker.hoursWorked = hours;
    picker.hours = hours;
}

/**
 * Simulate break times
 */
export function simulateBreaks(picker: SimulationPicker, hasBreaks: boolean): void {
    if (!hasBreaks) return;

    const now = new Date();
    // Assume shift started N hours ago
    const shiftStart = new Date(now.getTime() - picker.hoursWorked * 3600000);

    // Last rest break was ~2.5 hours ago
    if (picker.hoursWorked >= 2) {
        picker.lastBreakTime = new Date(shiftStart.getTime() + (picker.hoursWorked - 2.5) * 3600000);
    }

    // Last meal break was ~4.5 hours ago
    if (picker.hoursWorked >= 4) {
        picker.lastMealTime = new Date(shiftStart.getTime() + (picker.hoursWorked - 4.5) * 3600000);
    }
}

/**
 * Inject pickers into store (simulation mode)
 */
export async function injectTestData(pickers: SimulationPicker[]): Promise<void> {
    // Access the store dynamically
    const { useHarvestStore } = await import('../src/stores/useHarvestStore');
    const store = useHarvestStore.getState();

    // Enable simulation mode
    store.setSimulationMode(true);

    // Inject crew data
    store.setGlobalState({
        crew: pickers as Picker[]
    });

    // Recalculate intelligence
    store.recalculateIntelligence();

    console.log(`🧪 [Simulator] Injected ${pickers.length} test pickers into store`);
}

/**
 * Clear simulation data
 */
export async function clearSimulation(): Promise<void> {
    const { useHarvestStore } = await import('../src/stores/useHarvestStore');
    const store = useHarvestStore.getState();

    store.setSimulationMode(false);
    store.setGlobalState({
        crew: [],
        alerts: [],
        payroll: { totalPiece: 0, totalMinimum: 0, finalTotal: 0 }
    });

    console.log('🧪 [Simulator] Simulation mode disabled and data cleared');
}

export const shiftSimulator = {
    generateMockPickers,
    simulateBucketScans,
    simulateBreaks,
    injectTestData,
    clearSimulation
};
