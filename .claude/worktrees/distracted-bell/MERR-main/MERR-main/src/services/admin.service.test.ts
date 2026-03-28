import { describe, it, expect, vi, beforeEach } from 'vitest';
import { adminService } from './admin.service';

// ── Mocks ──────────────────────────────────
vi.mock('@/utils/logger', () => ({
    logger: { info: vi.fn(), warn: vi.fn(), error: vi.fn() },
}));
vi.mock('./supabase', () => ({
    supabase: { from: vi.fn() },
}));

import { supabase } from './supabase';

const mockFrom = supabase.from as ReturnType<typeof vi.fn>;

describe('adminService', () => {
    beforeEach(() => vi.clearAllMocks());

    // ═══════════════════════════════════════
    // getAllOrchards
    // ═══════════════════════════════════════

    it('getAllOrchards returns enriched orchards on success', async () => {
        const dbRows = [
            { id: 'o1', name: 'Alpha Orchard', total_rows: 50 },
            { id: 'o2', name: 'Beta Orchard', total_rows: null },
        ];
        mockFrom.mockReturnValue({
            select: () => ({ order: () => ({ data: dbRows, error: null }) }),
        });

        const result = await adminService.getAllOrchards();

        expect(result).toHaveLength(2);
        expect(result[0]).toMatchObject({ id: 'o1', name: 'Alpha Orchard', total_rows: 50, active_pickers: 0, today_buckets: 0, compliance_score: 100 });
        expect(result[1].total_rows).toBe(0); // null → 0 fallback
    });

    it('getAllOrchards returns empty array on error', async () => {
        mockFrom.mockReturnValue({
            select: () => ({ order: () => ({ data: null, error: { message: 'DB error' } }) }),
        });

        const result = await adminService.getAllOrchards();
        expect(result).toEqual([]);
    });

    // ═══════════════════════════════════════
    // getAllUsers
    // ═══════════════════════════════════════

    it('getAllUsers returns users without filters', async () => {
        const users = [{ id: 'u1', email: 'a@b.com', full_name: 'Alice', role: 'picker', is_active: true, orchard_id: 'o1', created_at: '2026-01-01' }];
        mockFrom.mockReturnValue({
            select: () => ({ order: () => ({ data: users, error: null }) }),
        });

        const result = await adminService.getAllUsers();
        expect(result).toEqual(users);
    });

    it('getAllUsers applies role filter', async () => {
        const mockEq = vi.fn().mockReturnValue({ data: [], error: null });
        const mockOrder = vi.fn().mockReturnValue({ eq: mockEq });
        mockFrom.mockReturnValue({
            select: () => ({ order: mockOrder }),
        });

        await adminService.getAllUsers({ role: 'manager' });

        expect(mockEq).toHaveBeenCalledWith('role', 'manager');
    });

    it('getAllUsers returns empty array on error', async () => {
        mockFrom.mockReturnValue({
            select: () => ({ order: () => ({ data: null, error: { message: 'Crash' } }) }),
        });

        const result = await adminService.getAllUsers();
        expect(result).toEqual([]);
    });

    // ═══════════════════════════════════════
    // updateUserRole
    // ═══════════════════════════════════════

    it('updateUserRole returns true on success', async () => {
        mockFrom.mockReturnValue({
            update: () => ({ eq: () => ({ error: null }) }),
        });

        expect(await adminService.updateUserRole('u1', 'admin')).toBe(true);
    });

    it('updateUserRole returns false on error', async () => {
        mockFrom.mockReturnValue({
            update: () => ({ eq: () => ({ error: { message: 'Denied' } }) }),
        });

        expect(await adminService.updateUserRole('u1', 'admin')).toBe(false);
    });

    // ═══════════════════════════════════════
    // deactivateUser
    // ═══════════════════════════════════════

    it('deactivateUser sets is_active to false', async () => {
        const mockUpdate = vi.fn().mockReturnValue({ eq: () => ({ error: null }) });
        mockFrom.mockReturnValue({ update: mockUpdate });

        const result = await adminService.deactivateUser('u1');

        expect(result).toBe(true);
        expect(mockUpdate).toHaveBeenCalledWith(expect.objectContaining({ is_active: false }));
    });

    it('deactivateUser returns false on error', async () => {
        mockFrom.mockReturnValue({
            update: () => ({ eq: () => ({ error: { message: 'Error' } }) }),
        });

        expect(await adminService.deactivateUser('u1')).toBe(false);
    });

    // ═══════════════════════════════════════
    // reactivateUser
    // ═══════════════════════════════════════

    it('reactivateUser sets is_active to true', async () => {
        const mockUpdate = vi.fn().mockReturnValue({ eq: () => ({ error: null }) });
        mockFrom.mockReturnValue({ update: mockUpdate });

        const result = await adminService.reactivateUser('u1');

        expect(result).toBe(true);
        expect(mockUpdate).toHaveBeenCalledWith(expect.objectContaining({ is_active: true }));
    });

    it('reactivateUser returns false on error', async () => {
        mockFrom.mockReturnValue({
            update: () => ({ eq: () => ({ error: { message: 'Error' } }) }),
        });

        expect(await adminService.reactivateUser('u1')).toBe(false);
    });
});
