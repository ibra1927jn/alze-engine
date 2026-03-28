/**
 * Fraud Detection Service — Frontend Bridge to Server-Side Engine
 * 
 * SECURITY: Real fraud detection runs server-side in the Edge Function
 * `detect-anomalies`. This service acts as a bridge, calling the backend
 * and falling back to mock data when offline.
 * 
 * Three core principles implemented server-side:
 * 1. ELAPSED TIME VELOCITY — buckets ÷ elapsed time since last collection
 * 2. PEER COMPARISON — compare picker to peers in same row
 * 3. GRACE PERIOD — first 90 min = warmup, only extreme outliers flagged
 */

import { edgeFunctionsRepository } from '@/repositories/edgeFunctions.repository';

export type AnomalyType =
    | 'impossible_velocity'
    | 'peer_outlier'
    | 'off_hours'
    | 'duplicate_proximity'
    | 'post_collection_spike';

export interface Anomaly {
    id: string;
    type: AnomalyType;
    severity: 'low' | 'medium' | 'high';
    pickerId: string;
    pickerName: string;
    detail: string;
    timestamp: string;
    evidence: Record<string, unknown>;
    rule: 'elapsed_velocity' | 'peer_comparison' | 'grace_period_exempt' | 'off_hours' | 'duplicate';
}

export interface DetectionConfig {
    gracePeriodMinutes: number;
    peerOutlierThreshold: number;
    maxPhysicalRate: number;
    postCollectionWindowMinutes: number;
    shiftStartHour: number;
    shiftEndHour: number;
}

const DEFAULT_CONFIG: DetectionConfig = {
    gracePeriodMinutes: 90,
    peerOutlierThreshold: 3.0,
    maxPhysicalRate: 8,
    postCollectionWindowMinutes: 20,
    shiftStartHour: 6,
    shiftEndHour: 19,
};

export const fraudDetectionService = {
    config: { ...DEFAULT_CONFIG },

    /**
     * Fetch anomalies from the backend Edge Function.
     * Falls back to mock data if offline or Edge Function unavailable.
     */
    fetchAnomalies: async (orchardId: string): Promise<Anomaly[]> => {
        try {
            const { data, error } = await edgeFunctionsRepository.invoke('detect-anomalies', {
                orchard_id: orchardId,
            });

            if (error) {
                console.warn('[FraudDetection] Edge Function error, using mock data:', error.message);
                return fraudDetectionService.getMockAnomalies();
            }

            return (data as Record<string, unknown>)?.anomalies as Anomaly[] || [];
        } catch (err) {
            console.warn('[FraudDetection] Network error, using mock data:', err);
            return fraudDetectionService.getMockAnomalies();
        }
    },

    /**
     * Analyze bucket records using smart rules.
     * @deprecated Use fetchAnomalies() instead — this runs client-side.
     */
    analyzeRecords: (_bucketRecords: unknown[], _crew: unknown[]): Anomaly[] => {
        return [];
    },

    /**
     * Get mock anomalies that demonstrate the SMART detection logic.
     * Used as offline fallback and for demo/development purposes.
     */
    getMockAnomalies: (): Anomaly[] => {
        const now = new Date();

        return [
            {
                id: 'anm-001',
                type: 'post_collection_spike',
                severity: 'high',
                pickerId: 'p-1',
                pickerName: 'Sione Tupou',
                detail: '5 buckets appeared 15 min after runner collected all his stock. Rate: 20/hr (max physical: 8/hr). Possible buddy punching.',
                timestamp: new Date(now.getTime() - 15 * 60000).toISOString(),
                evidence: {
                    bucketsAfterPickup: 5,
                    minutesSinceCollection: 15,
                    impliedRate: 20,
                    maxPhysicalRate: 8,
                    lastCollectionTime: new Date(now.getTime() - 30 * 60000).toISOString(),
                },
                rule: 'elapsed_velocity',
            },
            {
                id: 'anm-002',
                type: 'peer_outlier',
                severity: 'high',
                pickerId: 'p-3',
                pickerName: 'John Doe',
                detail: '12.5 bins/hr while row mates average 3.0/hr (4.2× faster). Same trees, same conditions — only he is racing.',
                timestamp: new Date(now.getTime() - 120 * 60000).toISOString(),
                evidence: {
                    pickerRate: 12.5,
                    rowAverage: 3.0,
                    rowPeers: ['María López (3.2)', 'Pedro Reyes (2.8)', 'Ana Tonga (3.0)'],
                    multiplier: 4.2,
                    rowId: 'Row 7',
                    blockName: 'Block C',
                },
                rule: 'peer_comparison',
            },
            {
                id: 'anm-003',
                type: 'off_hours',
                severity: 'medium',
                pickerId: 'p-2',
                pickerName: 'Maria Garcia',
                detail: 'Scan at 04:15 AM — 1h 45min before shift start (06:00). Equipment may have been used without authorization.',
                timestamp: new Date(new Date().setHours(4, 15, 0)).toISOString(),
                evidence: {
                    scanTime: '04:15 AM',
                    shiftStart: '06:00 AM',
                    minutesBeforeShift: 105,
                },
                rule: 'off_hours',
            },
            {
                id: 'anm-004',
                type: 'duplicate_proximity',
                severity: 'medium',
                pickerId: 'p-4',
                pickerName: 'David Smith',
                detail: 'Scanned bin #B-2847 — same bin was already scanned by Sione Tupou 45s earlier. Possible tag sharing.',
                timestamp: new Date(now.getTime() - 45 * 60000).toISOString(),
                evidence: {
                    binId: 'B-2847',
                    conflictPicker: 'Sione Tupou',
                    timeDiffSeconds: 45,
                    rowId: 'Row 12',
                },
                rule: 'duplicate',
            },
            {
                id: 'anm-005',
                type: 'impossible_velocity',
                severity: 'high',
                pickerId: 'p-5',
                pickerName: 'Ana Tonga',
                detail: '7 buckets in first 20 min of work (21/hr). Grace period had not expired — flagged as impossible velocity regardless of warmup.',
                timestamp: new Date(now.getTime() - 60 * 60000).toISOString(),
                evidence: {
                    buckets: 7,
                    minutesWorked: 20,
                    impliedRate: 21,
                    maxPhysicalRate: 8,
                    shiftStarted: new Date(now.getTime() - 80 * 60000).toISOString(),
                    note: 'Grace period suppresses normal velocity alerts, but impossible rates (>2.5× physical max) always trigger.',
                },
                rule: 'elapsed_velocity',
            },
            {
                id: 'anm-006',
                type: 'peer_outlier',
                severity: 'low',
                pickerId: 'p-6',
                pickerName: 'Liam Parker',
                detail: '7.5 bins/hr vs row average 3.1/hr (2.4× — below threshold). Could be a skilful picker or favorable tree position. Monitoring.',
                timestamp: new Date(now.getTime() - 180 * 60000).toISOString(),
                evidence: {
                    pickerRate: 7.5,
                    rowAverage: 3.1,
                    multiplier: 2.4,
                    rowId: 'Row 3',
                    blockName: 'Block A',
                    status: 'monitoring',
                },
                rule: 'peer_comparison',
            },
        ];
    },

    /**
     * Explains to the manager WHY a burst of scans is NOT fraud.
     * Used to build trust in the system by showing what it dismissed.
     */
    getDismissedExamples: (): { scenario: string; reason: string; rule: string }[] => [
        {
            scenario: 'Runner scanned 6 buckets for Juan in 10 seconds',
            reason: 'Juan worked 2 hours since last collection. 6 ÷ 2h = 3/hr — perfectly normal rate. Buckets were accumulated under trees.',
            rule: 'elapsed_velocity',
        },
        {
            scenario: 'Entire Row 5 was picking at 2× average speed today',
            reason: 'All 8 pickers in Row 5 showed elevated rates. Trees in that section are heavily loaded this season. Group elevation = good trees, not fraud.',
            rule: 'peer_comparison',
        },
        {
            scenario: 'María had zero scans for first 75 minutes of shift',
            reason: 'Grace period (first 90 min) — workers set up ladders, fruit is cold, tractors haven\'t started yet. Normal warmup. No alert generated.',
            rule: 'grace_period',
        },
    ],
};
