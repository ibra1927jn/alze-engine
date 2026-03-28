import { withOptimisticLock } from '../optimistic-lock.service';
import { nowNZST } from '@/utils/nzst';
import type { TransportPayload } from './types';
import { transportRequestRepo } from '@/repositories/index';

/**
 * Process transport request sync items — create, assign, or complete.
 * Supports optimistic locking on assign/complete when updated_at is available.
 */
export async function processTransport(payload: TransportPayload, expectedUpdatedAt?: string): Promise<void> {
    if (payload.action === 'create') {
        await transportRequestRepo.create({
            orchard_id: payload.orchard_id!,
            requested_by: payload.requested_by!,
            requester_name: payload.requester_name || 'Unknown',
            zone: payload.zone!,
            bins_count: payload.bins_count || 1,
            priority: (payload.priority || 'normal') as 'normal' | 'high' | 'urgent',
            notes: payload.notes || null,
        });
    } else if (payload.action === 'assign' && payload.requestId) {
        const updates = {
            assigned_vehicle: payload.vehicleId,
            assigned_by: payload.assignedBy,
            status: 'assigned' as const,
        };
        if (expectedUpdatedAt) {
            const result = await withOptimisticLock({
                table: 'transport_requests',
                recordId: payload.requestId,
                expectedUpdatedAt,
                updates,
            });
            if (!result.success) {
                throw new Error(`Optimistic lock conflict on transport ${payload.requestId}`);
            }
        } else {
            await transportRequestRepo.update(payload.requestId, updates);
        }
    } else if (payload.action === 'complete' && payload.requestId) {
        const updates = {
            status: 'completed' as const,
            completed_at: nowNZST(),
        };
        if (expectedUpdatedAt) {
            const result = await withOptimisticLock({
                table: 'transport_requests',
                recordId: payload.requestId,
                expectedUpdatedAt,
                updates,
            });
            if (!result.success) {
                throw new Error(`Optimistic lock conflict on transport ${payload.requestId}`);
            }
        } else {
            await transportRequestRepo.update(payload.requestId, updates);
        }
    }
}
