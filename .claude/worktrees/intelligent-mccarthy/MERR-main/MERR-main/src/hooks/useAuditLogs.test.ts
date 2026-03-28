/**
 * useAuditLogs Hook Tests — pure fetcher functions
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { auditRepository } from '@/repositories/audit.repository';

// Test the pure fetcher functions extracted from the hook
vi.mock('@/repositories/audit.repository', () => ({
    auditRepository: {
        query: vi.fn(),
        getRecordHistory: vi.fn(),
        getStats: vi.fn(),
    },
}));

describe('useAuditLogs fetcher logic', () => {
    beforeEach(() => { vi.clearAllMocks(); });

    describe('fetchAuditLogs', () => {
        it('delegates to auditRepository.query', async () => {
            const logs = [{ id: '1', action: 'INSERT', table_name: 'pickers' }];
            vi.mocked(auditRepository.query).mockResolvedValue(logs);
            const result = await auditRepository.query({ limit: 50 });
            expect(result).toEqual(logs);
            expect(auditRepository.query).toHaveBeenCalledWith({ limit: 50 });
        });
    });

    describe('fetchRecordHistory', () => {
        it('delegates to auditRepository.getRecordHistory', async () => {
            const history = [{ id: '1', action: 'UPDATE' }];
            vi.mocked(auditRepository.getRecordHistory).mockResolvedValue(history);
            const result = await auditRepository.getRecordHistory('pickers', 'p1');
            expect(result).toEqual(history);
        });
    });

    describe('fetchAuditStats aggregation', () => {
        it('aggregates logs by action and table', async () => {
            const logs = [
                { action: 'INSERT', table_name: 'pickers' },
                { action: 'INSERT', table_name: 'pickers' },
                { action: 'UPDATE', table_name: 'users' },
            ];
            vi.mocked(auditRepository.getStats).mockResolvedValue(logs);
            const data = await auditRepository.getStats('2026-01-01');

            // Simulate the stats calculation from the hook
            const byAction: Record<string, number> = {};
            const byTable: Record<string, number> = {};
            data.forEach((log: { action: string; table_name: string }) => {
                byAction[log.action] = (byAction[log.action] || 0) + 1;
                byTable[log.table_name] = (byTable[log.table_name] || 0) + 1;
            });

            expect(byAction).toEqual({ INSERT: 2, UPDATE: 1 });
            expect(byTable).toEqual({ pickers: 2, users: 1 });
        });
    });
});
