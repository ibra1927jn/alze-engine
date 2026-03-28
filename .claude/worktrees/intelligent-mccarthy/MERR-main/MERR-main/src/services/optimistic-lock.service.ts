/**
 * Optimistic Lock Service
 * =======================
 * Atomic optimistic locking via Supabase WHERE clause.
 *
 * Pattern:
 *   UPDATE ... SET ... WHERE id = $id AND updated_at = $expected
 *
 * - Happy path (99%): 1 network trip — atomic UPDATE succeeds
 * - Conflict path (1%): 2 trips — failed UPDATE + SELECT to get winner's state
 *
 * The `set_updated_at()` DB trigger auto-bumps `updated_at` on every UPDATE,
 * so subsequent operations will always see a fresh timestamp.
 */

import { optimisticLockRepository } from '@/repositories/optimisticLock.repository';
import { conflictService, type SyncConflict } from './conflict.service';
import { logger } from '@/utils/logger';

export interface OptimisticLockOptions {
    /** DB table name (e.g. 'pickers', 'contracts') */
    table: string;
    /** UUID of the record to update */
    recordId: string;
    /** The `updated_at` value the client last read */
    expectedUpdatedAt: string;
    /** The fields to update */
    updates: Record<string, unknown>;
}

export interface OptimisticLockResult {
    success: boolean;
    /** The updated row (only on success) */
    data?: Record<string, unknown>;
    /** Conflict details (only on failure) */
    conflict?: SyncConflict;
}

/**
 * Perform an atomic optimistic-locked update.
 */
export async function withOptimisticLock(
    options: OptimisticLockOptions
): Promise<OptimisticLockResult> {
    const { table, recordId, expectedUpdatedAt, updates } = options;

    // ── STEP 1: Atomic conditional update ──────────────────
    const { data, error } = await optimisticLockRepository.conditionalUpdate(
        table, recordId, expectedUpdatedAt, updates
    );

    // ── STEP 2: No rows returned → version mismatch ───────
    if (error && error.code === 'PGRST116') {
        logger.warn(
            `[OptimisticLock] ⚠️ Conflict on ${table}/${recordId}: ` +
            `expected updated_at=${expectedUpdatedAt}, but row was modified by another client`
        );

        // Spend a second trip to retrieve the winner's state
        const serverState = await optimisticLockRepository.getById(table, recordId);

        if (!serverState) {
            logger.error(`[OptimisticLock] Record ${table}/${recordId} not found — may have been deleted`);
            const conflict = await conflictService.detect(
                table, recordId, expectedUpdatedAt, 'DELETED', updates, {}
            );
            return { success: false, conflict: conflict ?? undefined };
        }

        const conflict = await conflictService.detect(
            table, recordId, expectedUpdatedAt,
            serverState.updated_at ?? 'unknown', updates, serverState
        );

        return { success: false, conflict: conflict ?? undefined };
    }

    // ── STEP 3: Any other error (network, RLS, constraint) ──
    if (error) {
        throw error;
    }

    // ── STEP 4: Success ────────────────────────────────────
    logger.info(
        `[OptimisticLock] ✅ Atomic update on ${table}/${recordId} succeeded`
    );

    return { success: true, data: data ?? undefined };
}

/**
 * Perform a standard (non-locked) update.
 */
export async function updateWithoutLock(
    table: string,
    recordId: string,
    updates: Record<string, unknown>
): Promise<{ success: boolean; data?: Record<string, unknown> }> {
    const data = await optimisticLockRepository.update(table, recordId, updates);
    return { success: true, data: data ?? undefined };
}

