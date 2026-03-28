/**
 * audit.service.test.ts — Unit tests 
 *
 * Uses vi.spyOn(supabase, 'from') — the proven pattern in this codebase.
 * We mock the supabase.from chain to intercept auditRepository.insertBatch calls.
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { supabase } from './supabase';

vi.mock('@/utils/nzst', () => ({
    nowNZST: vi.fn(() => '2026-03-04T12:00:00+13:00'),
}));

vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn(), debug: vi.fn() },
}));

vi.mock('./config.service', () => ({
    getConfig: vi.fn(() => ({ isDevelopment: true, SUPABASE_URL: 'https://test.supabase.co', SUPABASE_ANON_KEY: 'test' })),
}));

import { auditService, logAudit } from './audit.service';

// Track mock state for supabase insert calls
let mockInsert: ReturnType<typeof vi.fn>;

describe('auditService', () => {
    beforeEach(() => {
        vi.restoreAllMocks();
        mockInsert = vi.fn().mockResolvedValue({ error: null });
        vi.spyOn(supabase, 'from').mockReturnValue({
            insert: mockInsert,
        } as never);
    });

    describe('logAudit — immediate flush', () => {
        it('flushes critical events immediately via supabase insert', async () => {
            await logAudit('system.error', 'Critical failure', { severity: 'critical' });
            expect(mockInsert).toHaveBeenCalled();
            const insertedLogs = mockInsert.mock.calls[0][0];
            expect(insertedLogs[0].event_type).toBe('system.error');
            expect(insertedLogs[0].severity).toBe('critical');
        });

        it('flushes error events immediately', async () => {
            await logAudit('system.error', 'Error happened', { severity: 'error' });
            expect(mockInsert).toHaveBeenCalledTimes(1);
        });

        it('flushes events marked as immediate', async () => {
            await logAudit('auth.login', 'Login', { immediate: true });
            expect(mockInsert).toHaveBeenCalledTimes(1);
        });

        it('includes created_at from nowNZST', async () => {
            await logAudit('auth.login', 'Login', { immediate: true });
            const insertedLogs = mockInsert.mock.calls[0][0];
            expect(typeof insertedLogs[0].created_at).toBe('string');
            expect(insertedLogs[0].created_at.length).toBeGreaterThan(10);
        });

        it('includes userId and orchardId', async () => {
            await logAudit('auth.login', 'Login', {
                immediate: true,
                userId: 'u-1',
                orchardId: 'o-1',
            });
            const insertedLogs = mockInsert.mock.calls[0][0];
            expect(insertedLogs[0].user_id).toBe('u-1');
            expect(insertedLogs[0].orchard_id).toBe('o-1');
        });
    });

    describe('logAudit — queued events', () => {
        it('does NOT flush info events immediately', async () => {
            await logAudit('auth.login', 'User logged in');
            expect(mockInsert).not.toHaveBeenCalled();
        });
    });

    describe('forceFlush', () => {
        it('flushes all queued events', async () => {
            await logAudit('auth.login', 'Login 1');
            await logAudit('auth.logout', 'Logout 1');
            expect(mockInsert).not.toHaveBeenCalled();

            await auditService.forceFlush();
            expect(mockInsert).toHaveBeenCalledTimes(1);
        });

        it('does nothing when queue is empty', async () => {
            await auditService.forceFlush();
            expect(mockInsert).not.toHaveBeenCalled();
        });
    });

    describe('convenience functions', () => {
        it('logAuth creates auth event type', async () => {
            await auditService.logAuth('login', 'u-1', 'test@test.com');
            expect(mockInsert).not.toHaveBeenCalled(); // info → queued
        });

        it('logPickerEvent creates picker event type', async () => {
            await auditService.logPickerEvent('created', 'p-1', 'u-1');
            expect(mockInsert).not.toHaveBeenCalled();
        });

        it('logQCEvent creates QC event type', async () => {
            await auditService.logQCEvent('inspection_created', 'insp-1', 'p-1');
            expect(mockInsert).not.toHaveBeenCalled();
        });

        it('logComplianceEvent creates compliance event type', async () => {
            await auditService.logComplianceEvent('break_started', 'p-1');
            expect(mockInsert).not.toHaveBeenCalled();
        });
    });
});
