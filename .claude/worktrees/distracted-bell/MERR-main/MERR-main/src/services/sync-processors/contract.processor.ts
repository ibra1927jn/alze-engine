import { withOptimisticLock } from '../optimistic-lock.service';
import type { ContractPayload } from './types';
import { contractRepo } from '@/repositories/index';

/**
 * Process contract sync items — create or update contracts in Supabase.
 * Supports optimistic locking on updates when updated_at is available.
 */
export async function processContract(payload: ContractPayload, expectedUpdatedAt?: string): Promise<void> {
    if (payload.action === 'create') {
        await contractRepo.create({
            employee_id: payload.employee_id!,
            orchard_id: payload.orchard_id!,
            type: payload.type as 'permanent' | 'seasonal' | 'casual',
            start_date: payload.start_date!,
            end_date: payload.end_date || null,
            hourly_rate: payload.hourly_rate || 23.50,
            notes: payload.notes || null,
        });
    } else if (payload.action === 'update' && payload.contractId) {
        const updates: Record<string, unknown> = {};
        if (payload.status !== undefined) updates.status = payload.status;
        if (payload.end_date !== undefined) updates.end_date = payload.end_date;
        if (payload.hourly_rate !== undefined) updates.hourly_rate = payload.hourly_rate;
        if (payload.notes !== undefined) updates.notes = payload.notes;

        if (expectedUpdatedAt) {
            const result = await withOptimisticLock({
                table: 'contracts',
                recordId: payload.contractId,
                expectedUpdatedAt,
                updates,
            });
            if (!result.success) {
                throw new Error(`Optimistic lock conflict on contract ${payload.contractId}`);
            }
        } else {
            await contractRepo.update(payload.contractId, updates);
        }
    }
}
