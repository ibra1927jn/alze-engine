/**
 * Dead Letter Queue Types
 * Extracted from DeadLetterQueueView.tsx for reuse in tests and services.
 * @module components/views/manager/deadLetterQueue.types
 */
import type { QueuedSyncItem } from '@/services/db';

export interface DeadLetterItem {
    id: string;
    type: QueuedSyncItem['type'];
    payload: Record<string, unknown>;
    timestamp: number;
    retryCount: number;
    failureReason?: string;
    errorCode?: string;
    movedAt?: number;
}

export interface CategorizedErrors {
    critical: DeadLetterItem[];
    warnings: DeadLetterItem[];
    recent: DeadLetterItem[];
}

/**
 * Human-readable error tooltip based on Supabase/Postgres error codes.
 * Pure function — no side effects.
 */
export function getErrorTooltip(item: DeadLetterItem): string {
    const errorCode = item.errorCode;
    const errorMsg = item.failureReason;

    // Common error explanations
    if (errorCode?.includes('23503')) {
        return '❌ Foreign Key Violation: Picker or orchard no longer exists in database';
    }
    if (errorCode?.includes('23505')) {
        return '⚠️ Duplicate: This record already exists in the database';
    }
    if (errorCode?.includes('PGRST116')) {
        return '🔒 RLS Policy Violation: Action blocked by database security rules (e.g., archived picker)';
    }
    if (errorMsg?.includes('archived')) {
        return '🚫 Picker Archived: Cannot sync buckets for removed/suspended workers';
    }
    if (errorMsg?.includes('Network')) {
        return '📡 Network Error: Connection lost during sync';
    }
    return errorMsg || 'Unknown error';
}
