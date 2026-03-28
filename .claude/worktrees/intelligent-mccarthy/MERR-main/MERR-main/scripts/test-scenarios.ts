/**
 * test-scenarios.ts
 * Predefined test scenarios for compliance and payroll testing
 */

import { shiftSimulator, SimulationResult } from './shift-simulator';

const ORCHARD_ID = 'test-orchard-1';

/**
 * Scenario 1: Normal Healthy Shift
 * All pickers performing well, proper breaks, above minimum wage
 */
export async function runScenario1_NormalShift(): Promise<SimulationResult> {
    const startTime = Date.now();
    const errors: string[] = [];

    try {
        // Generate 5 pickers
        const pickers = shiftSimulator.generateMockPickers(5, ORCHARD_ID);

        // Simulate healthy productivity (5 buckets/hr, 4 hours worked)
        pickers.forEach(picker => {
            shiftSimulator.simulateBucketScans(picker, 5.0, 4.0);
            shiftSimulator.simulateBreaks(picker, true);
        });

        // Inject into store
        await shiftSimulator.injectTestData(pickers);

        // Get results
        const { useHarvestStore } = await import('../src/stores/useHarvestStore');
        const state = useHarvestStore.getState();

        return {
            scenario: 'Scenario 1: Normal Healthy Shift',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers,
            violations: state.alerts || [],
            payroll: state.payroll || { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: true,
            errors
        };
    } catch (error) {
        errors.push(String(error));
        return {
            scenario: 'Scenario 1: Normal Healthy Shift',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers: [],
            violations: [],
            payroll: { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: false,
            errors
        };
    }
}

/**
 * Scenario 2: Below Minimum Wage
 * Picker falls below minimum wage at ~2 hour mark
 */
export async function runScenario2_BelowMinimum(): Promise<SimulationResult> {
    const startTime = Date.now();
    const errors: string[] = [];

    try {
        const pickers = shiftSimulator.generateMockPickers(3, ORCHARD_ID);

        // Picker 1: Good performance
        shiftSimulator.simulateBucketScans(pickers[0], 5.0, 4.0);
        shiftSimulator.simulateBreaks(pickers[0], true);

        // Picker 2: LOW performance - below minimum wage
        shiftSimulator.simulateBucketScans(pickers[1], 2.5, 4.0); // ~2.5 bkt/hr = $16.25/hr (below $23.50)
        shiftSimulator.simulateBreaks(pickers[1], true);

        // Picker 3: Average
        shiftSimulator.simulateBucketScans(pickers[2], 4.0, 4.0);
        shiftSimulator.simulateBreaks(pickers[2], true);

        await shiftSimulator.injectTestData(pickers);

        const { useHarvestStore } = await import('../src/stores/useHarvestStore');
        const state = useHarvestStore.getState();

        return {
            scenario: 'Scenario 2: Below Minimum Wage',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers,
            violations: state.alerts || [],
            payroll: state.payroll || { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: true,
            errors
        };
    } catch (error) {
        errors.push(String(error));
        return {
            scenario: 'Scenario 2: Below Minimum Wage',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers: [],
            violations: [],
            payroll: { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: false,
            errors
        };
    }
}

/**
 * Scenario 3: Missed Breaks
 * Picker working without proper rest/meal breaks
 */
export async function runScenario3_MissedBreaks(): Promise<SimulationResult> {
    const startTime = Date.now();
    const errors: string[] = [];

    try {
        const pickers = shiftSimulator.generateMockPickers(2, ORCHARD_ID);

        // Picker 1: Good performance, proper breaks
        shiftSimulator.simulateBucketScans(pickers[0], 5.0, 5.0);
        shiftSimulator.simulateBreaks(pickers[0], true);

        // Picker 2: NO BREAKS - violation
        shiftSimulator.simulateBucketScans(pickers[1], 4.5, 5.0);
        shiftSimulator.simulateBreaks(pickers[1], false); // No breaks!

        await shiftSimulator.injectTestData(pickers);

        const { useHarvestStore } = await import('../src/stores/useHarvestStore');
        const state = useHarvestStore.getState();

        return {
            scenario: 'Scenario 3: Missed Breaks',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers,
            violations: state.alerts || [],
            payroll: state.payroll || { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: true,
            errors
        };
    } catch (error) {
        errors.push(String(error));
        return {
            scenario: 'Scenario 3: Missed Breaks',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers: [],
            violations: [],
            payroll: { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: false,
            errors
        };
    }
}

/**
 * Scenario 4: Edge Cases
 * New picker (0 hours), extreme productivity
 */
export async function runScenario4_EdgeCases(): Promise<SimulationResult> {
    const startTime = Date.now();
    const errors: string[] = [];

    try {
        const pickers = shiftSimulator.generateMockPickers(3, ORCHARD_ID);

        // Picker 1: Brand new (0 hours worked)
        shiftSimulator.simulateBucketScans(pickers[0], 0, 0);
        shiftSimulator.simulateBreaks(pickers[0], false);

        // Picker 2: Extreme HIGH productivity
        shiftSimulator.simulateBucketScans(pickers[1], 8.0, 4.0);
        shiftSimulator.simulateBreaks(pickers[1], true);

        // Picker 3: Very LOW  productivity
        shiftSimulator.simulateBucketScans(pickers[2], 1.0, 4.0);
        shiftSimulator.simulateBreaks(pickers[2], true);

        await shiftSimulator.injectTestData(pickers);

        const { useHarvestStore } = await import('../src/stores/useHarvestStore');
        const state = useHarvestStore.getState();

        return {
            scenario: 'Scenario 4: Edge Cases',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers,
            violations: state.alerts || [],
            payroll: state.payroll || { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: true,
            errors
        };
    } catch (error) {
        errors.push(String(error));
        return {
            scenario: 'Scenario 4: Edge Cases',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers: [],
            violations: [],
            payroll: { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: false,
            errors
        };
    }
}

/**
 * Scenario 5: Mixed Team Performance
 * Realistic team with varied compliance states
 */
export async function runScenario5_MixedTeam(): Promise<SimulationResult> {
    const startTime = Date.now();
    const errors: string[] = [];

    try {
        const pickers = shiftSimulator.generateMockPickers(6, ORCHARD_ID);

        // Mix of performance levels
        shiftSimulator.simulateBucketScans(pickers[0], 6.0, 4.5);
        shiftSimulator.simulateBreaks(pickers[0], true);

        shiftSimulator.simulateBucketScans(pickers[1], 3.0, 4.5); // Below min
        shiftSimulator.simulateBreaks(pickers[1], false); // No breaks

        shiftSimulator.simulateBucketScans(pickers[2], 4.5, 4.5);
        shiftSimulator.simulateBreaks(pickers[2], true);

        shiftSimulator.simulateBucketScans(pickers[3], 2.0, 4.5); // Way below
        shiftSimulator.simulateBreaks(pickers[3], true);

        shiftSimulator.simulateBucketScans(pickers[4], 5.5, 4.5);
        shiftSimulator.simulateBreaks(pickers[4], true);

        shiftSimulator.simulateBucketScans(pickers[5], 3.5, 4.5); // Slightly below
        shiftSimulator.simulateBreaks(pickers[5], false);

        await shiftSimulator.injectTestData(pickers);

        const { useHarvestStore } = await import('../src/stores/useHarvestStore');
        const state = useHarvestStore.getState();

        return {
            scenario: 'Scenario 5: Mixed Team Performance',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers,
            violations: state.alerts || [],
            payroll: state.payroll || { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: true,
            errors
        };
    } catch (error) {
        errors.push(String(error));
        return {
            scenario: 'Scenario 5: Mixed Team Performance',
            timestamp: new Date(),
            duration: Date.now() - startTime,
            pickers: [],
            violations: [],
            payroll: { totalPiece: 0, totalMinimum: 0, finalTotal: 0 },
            success: false,
            errors
        };
    }
}

export const testScenarios = {
    runScenario1_NormalShift,
    runScenario2_BelowMinimum,
    runScenario3_MissedBreaks,
    runScenario4_EdgeCases,
    runScenario5_MixedTeam
};
