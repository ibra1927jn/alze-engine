// =============================================
// FRAUD DETECTION SERVICE TESTS
// =============================================
import { describe, it, expect, vi, beforeEach } from 'vitest';

// We test the service by importing it — fetchAnomalies will be tested
// with mocked supabase, and mock data functions directly.
import { fraudDetectionService } from './fraud-detection.service';

describe('Fraud Detection Service', () => {
    // =============================================
    // MOCK DATA QUALITY
    // =============================================
    describe('getMockAnomalies', () => {
        it('should return an array of anomalies', () => {
            const anomalies = fraudDetectionService.getMockAnomalies();
            expect(Array.isArray(anomalies)).toBe(true);
            expect(anomalies.length).toBeGreaterThan(0);
        });

        it('should have valid structure for each anomaly', () => {
            const anomalies = fraudDetectionService.getMockAnomalies();
            for (const a of anomalies) {
                expect(a).toHaveProperty('id');
                expect(a).toHaveProperty('type');
                expect(a).toHaveProperty('severity');
                expect(a).toHaveProperty('pickerId');
                expect(a).toHaveProperty('pickerName');
                expect(a).toHaveProperty('detail');
                expect(a).toHaveProperty('timestamp');
                expect(a).toHaveProperty('evidence');
                expect(a).toHaveProperty('rule');
            }
        });

        it('should only contain valid anomaly types', () => {
            const validTypes = ['impossible_velocity', 'peer_outlier', 'off_hours', 'duplicate_proximity', 'post_collection_spike'];
            const anomalies = fraudDetectionService.getMockAnomalies();
            for (const a of anomalies) {
                expect(validTypes).toContain(a.type);
            }
        });

        it('should only contain valid severity levels', () => {
            const validSeverities = ['low', 'medium', 'high'];
            const anomalies = fraudDetectionService.getMockAnomalies();
            for (const a of anomalies) {
                expect(validSeverities).toContain(a.severity);
            }
        });

        it('should only contain valid rule types', () => {
            const validRules = ['elapsed_velocity', 'peer_comparison', 'grace_period_exempt', 'off_hours', 'duplicate'];
            const anomalies = fraudDetectionService.getMockAnomalies();
            for (const a of anomalies) {
                expect(validRules).toContain(a.rule);
            }
        });

        it('should have valid ISO timestamps', () => {
            const anomalies = fraudDetectionService.getMockAnomalies();
            for (const a of anomalies) {
                const date = new Date(a.timestamp);
                expect(date.toString()).not.toBe('Invalid Date');
            }
        });

        it('should have unique IDs', () => {
            const anomalies = fraudDetectionService.getMockAnomalies();
            const ids = anomalies.map(a => a.id);
            expect(new Set(ids).size).toBe(ids.length);
        });

        it('should contain at least one high severity anomaly', () => {
            const anomalies = fraudDetectionService.getMockAnomalies();
            const highSeverity = anomalies.filter(a => a.severity === 'high');
            expect(highSeverity.length).toBeGreaterThan(0);
        });

        it('should cover multiple anomaly types', () => {
            const anomalies = fraudDetectionService.getMockAnomalies();
            const types = new Set(anomalies.map(a => a.type));
            expect(types.size).toBeGreaterThan(2);
        });
    });

    // =============================================
    // DISMISSED EXAMPLES
    // =============================================
    describe('getDismissedExamples', () => {
        it('should return array of dismissed scenarios', () => {
            const dismissed = fraudDetectionService.getDismissedExamples();
            expect(Array.isArray(dismissed)).toBe(true);
            expect(dismissed.length).toBeGreaterThan(0);
        });

        it('should have valid structure', () => {
            const dismissed = fraudDetectionService.getDismissedExamples();
            for (const d of dismissed) {
                expect(d).toHaveProperty('scenario');
                expect(d).toHaveProperty('reason');
                expect(d).toHaveProperty('rule');
                expect(typeof d.scenario).toBe('string');
                expect(typeof d.reason).toBe('string');
                expect(typeof d.rule).toBe('string');
            }
        });

        it('should include the 3 core rules', () => {
            const dismissed = fraudDetectionService.getDismissedExamples();
            const rules = dismissed.map(d => d.rule);
            expect(rules).toContain('elapsed_velocity');
            expect(rules).toContain('peer_comparison');
            expect(rules).toContain('grace_period');
        });
    });

    // =============================================
    // DEPRECATED analyzeRecords
    // =============================================
    describe('analyzeRecords', () => {
        it('should return empty array (deprecated — logic moved to Edge Function)', () => {
            const result = fraudDetectionService.analyzeRecords([], []);
            expect(result).toEqual([]);
        });
    });

    // =============================================
    // fetchAnomalies (with mocked supabase)
    // =============================================
    describe('fetchAnomalies', () => {
        beforeEach(() => {
            vi.restoreAllMocks();
        });

        it('should fall back to mock data when supabase invoke fails', async () => {
            // Mock the supabase module to simulate a failure
            vi.mock('./supabase', () => ({
                supabase: {
                    functions: {
                        invoke: vi.fn().mockRejectedValue(new Error('Network error')),
                    },
                },
            }));

            const result = await fraudDetectionService.fetchAnomalies('test-orchard');
            expect(Array.isArray(result)).toBe(true);
            // Should get mock data as fallback
            expect(result.length).toBeGreaterThan(0);
        });
    });
});
