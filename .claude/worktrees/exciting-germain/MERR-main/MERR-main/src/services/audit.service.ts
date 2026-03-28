// =============================================
// AUDIT SERVICE - Security audit logging
// =============================================
import { nowNZST } from '@/utils/nzst';
import { getConfig } from './config.service';
import { logger } from '@/utils/logger';
import { auditRepository } from '@/repositories/audit.repository';

/**
 * Audit event types
 */
export type AuditEventType =
    // Authentication events
    | 'auth.login'
    | 'auth.logout'
    | 'auth.login_failed'
    | 'auth.password_reset'
    | 'auth.session_expired'
    // User management
    | 'user.created'
    | 'user.updated'
    | 'user.deleted'
    | 'user.role_changed'
    // Picker management
    | 'picker.created'
    | 'picker.updated'
    | 'picker.deleted'
    | 'picker.status_changed'
    | 'picker.assigned_row'
    // QC events
    | 'qc.inspection_created'
    | 'qc.grade_changed'
    // Bucket/Bin operations
    | 'bucket.scanned'
    | 'bucket.collected'
    | 'bin.created'
    | 'bin.completed'
    // Settings changes
    | 'settings.day_setup_created'
    | 'settings.day_setup_modified'
    | 'settings.broadcast_sent'
    // Compliance events
    | 'compliance.break_started'
    | 'compliance.break_ended'
    | 'compliance.violation_detected'
    // System events
    | 'system.error'
    | 'system.sync_completed';

/**
 * Audit log severity levels
 */
export type AuditSeverity = 'info' | 'warning' | 'error' | 'critical';

/**
 * Audit log entry structure
 */
export interface AuditLogEntry {
    id?: string;
    event_type: AuditEventType;
    severity: AuditSeverity;
    user_id?: string;
    user_email?: string;
    orchard_id?: string;
    entity_type?: string;
    entity_id?: string;
    action: string;
    details?: Record<string, unknown>;
    ip_address?: string;
    user_agent?: string;
    created_at?: string;
}

/**
 * In-memory queue for batch logging
 */
const logQueue: AuditLogEntry[] = [];
let flushTimeout: ReturnType<typeof setTimeout> | null = null;
const FLUSH_INTERVAL = 5000; // 5 seconds
const MAX_QUEUE_SIZE = 50;

/**
 * Get client information for audit logging
 */
function getClientInfo(): { ip_address?: string; user_agent?: string } {
    if (typeof window === 'undefined') return {};

    return {
        user_agent: navigator.userAgent,
        // IP address would be captured server-side, we mark as client-side
        ip_address: 'client-side',
    };
}

/**
 * Flush queued logs to database
 */
async function flushLogs(): Promise<void> {
    if (logQueue.length === 0) return;

    const logsToFlush = [...logQueue];
    logQueue.length = 0; // Clear queue

    try {
        await auditRepository.insertBatch(logsToFlush as unknown as Record<string, unknown>[]);
    } catch (err) {
        logger.error('[Audit] Error flushing logs:', err);
        // Re-queue failed logs (up to max size)
        logQueue.push(...logsToFlush.slice(0, MAX_QUEUE_SIZE - logQueue.length));
        if (getConfig().isDevelopment) {
            logger.debug('[Audit] Failed log entries:', logsToFlush);
        }
    }
}

/**
 * Schedule a flush if not already scheduled
 */
function scheduleFlush(): void {
    if (flushTimeout) return;

    flushTimeout = setTimeout(() => {
        flushTimeout = null;
        flushLogs();
    }, FLUSH_INTERVAL);
}

/**
 * Log an audit event
 */
export async function logAudit(
    eventType: AuditEventType,
    action: string,
    options: {
        severity?: AuditSeverity;
        userId?: string;
        userEmail?: string;
        orchardId?: string;
        entityType?: string;
        entityId?: string;
        details?: Record<string, unknown>;
        immediate?: boolean;
    } = {}
): Promise<void> {
    const {
        severity = 'info',
        userId,
        userEmail,
        orchardId,
        entityType,
        entityId,
        details,
        immediate = false,
    } = options;

    const entry: AuditLogEntry = {
        event_type: eventType,
        severity,
        action,
        user_id: userId,
        user_email: userEmail,
        orchard_id: orchardId,
        entity_type: entityType,
        entity_id: entityId,
        details,
        created_at: nowNZST(),
        ...getClientInfo(),
    };

    // Log to console in development
    if (getConfig().isDevelopment) {
        const level = severity === 'error' ? 'error' : severity === 'warning' ? 'warn' : 'info';
        logger[level](`[Audit] ${eventType}: ${action}`);
    }

    // Critical and error events should be logged immediately
    if (immediate || severity === 'critical' || severity === 'error') {
        try {
            await auditRepository.insertBatch([entry] as unknown as Record<string, unknown>[]);
        } catch (err) {
            logger.error('[Audit] Failed to log critical event:', err);
        }
        return;
    }

    // Add to queue for batch logging
    logQueue.push(entry);

    // Force flush if queue is full
    if (logQueue.length >= MAX_QUEUE_SIZE) {
        await flushLogs();
    } else {
        scheduleFlush();
    }
}

// =============================================
// CONVENIENCE FUNCTIONS
// =============================================

/**
 * Log authentication event
 */
export function logAuth(
    event: 'login' | 'logout' | 'login_failed' | 'password_reset' | 'session_expired',
    userId?: string,
    userEmail?: string,
    details?: Record<string, unknown>
): Promise<void> {
    return logAudit(
        `auth.${event}` as AuditEventType,
        `User ${event.replace('_', ' ')}`,
        {
            severity: event === 'login_failed' ? 'warning' : 'info',
            userId,
            userEmail,
            details,
        }
    );
}

/**
 * Log picker management event
 */
export function logPickerEvent(
    event: 'created' | 'updated' | 'deleted' | 'status_changed' | 'assigned_row',
    pickerId: string,
    userId?: string,
    details?: Record<string, unknown>
): Promise<void> {
    return logAudit(`picker.${event}` as AuditEventType, `Picker ${event.replace('_', ' ')}`, {
        entityType: 'picker',
        entityId: pickerId,
        userId,
        details,
    });
}

/**
 * Log QC event
 */
export function logQCEvent(
    event: 'inspection_created' | 'grade_changed',
    inspectionId: string,
    pickerId: string,
    userId?: string,
    details?: Record<string, unknown>
): Promise<void> {
    return logAudit(`qc.${event}` as AuditEventType, `QC ${event.replace('_', ' ')}`, {
        entityType: 'qc_inspection',
        entityId: inspectionId,
        userId,
        details: { ...details, pickerId },
    });
}

/**
 * Log compliance event
 */
export function logComplianceEvent(
    event: 'break_started' | 'break_ended' | 'violation_detected',
    pickerId: string,
    details?: Record<string, unknown>
): Promise<void> {
    return logAudit(
        `compliance.${event}` as AuditEventType,
        `Compliance ${event.replace('_', ' ')}`,
        {
            severity: event === 'violation_detected' ? 'warning' : 'info',
            entityType: 'picker',
            entityId: pickerId,
            details,
        }
    );
}

/**
 * Force flush all pending logs (call on app unload)
 */
export function forceFlush(): Promise<void> {
    if (flushTimeout) {
        clearTimeout(flushTimeout);
        flushTimeout = null;
    }
    return flushLogs();
}

// =============================================
// EXPORTS
// =============================================

export const auditService = {
    logAudit,
    logAuth,
    logPickerEvent,
    logQCEvent,
    logComplianceEvent,
    forceFlush,
};

export default auditService;