import Dexie, { Table } from 'dexie';
import { HarvestSettings, Picker } from '../types';

export interface QueuedBucket {
    id: string; // UUID from Store
    picker_id: string; // The UUID
    orchard_id: string; // CRUCIAL: No olvidar este campo
    quality_grade: 'A' | 'B' | 'C' | 'reject';
    timestamp: string;
    synced: number; // 0 = pending, 1 = synced, -1 = error
    scanned_by: string; // 🔧 V17: Who scanned this bucket (user UUID) — prevents authorship fraud
    row_number?: number;
    failure_reason?: string;
}

export interface QueuedMessage {
    id: string; // UUID
    channel_type: 'direct' | 'group' | 'team';
    recipient_id: string;
    sender_id: string;
    content: string;
    timestamp: string;
    synced: number; // 0 = pending, 1 = synced, -1 = error
    priority?: string;
    failure_reason?: string;
}

export interface CachedUser {
    id: string; // usually 'current' or 'roster_ORCHARDID'
    profile?: Picker; // Full user object (optional if storing roster)
    roster?: Picker[]; // Full roster list (optional if storing single user)
    orchard_id: string;
    timestamp: number;
}

export interface CachedSettings {
    id: string; // usually 'current'
    settings: HarvestSettings;
    timestamp: number;
}

export interface DeadLetterItem {
    id: string;
    type: string;
    payload: Record<string, unknown>;
    timestamp: number;
    retryCount: number;
    failureReason: string;
    errorCode?: string;
    movedAt: number; // when the item was moved to DLQ
}

/** Sync queue item — replaces localStorage queue (audit fix: no 5MB limit) */
export interface QueuedSyncItem {
    id: string;
    type: 'SCAN' | 'MESSAGE' | 'ATTENDANCE' | 'ASSIGNMENT' | 'CONTRACT' | 'TRANSPORT' | 'TIMESHEET' | 'PICKER' | 'QC_INSPECTION' | 'UNLINK';
    payload: Record<string, unknown>;
    timestamp: number;
    retryCount: number;
    updated_at?: string;
}

/** Metadata for sync state (last sync time, etc.) */
export interface SyncMeta {
    id: string;
    value: number;
}

/** Conflict records — replaces localStorage conflict storage */
export interface StoredConflict {
    id: string;
    table: string;
    record_id: string;
    local_updated_at: string;
    server_updated_at: string;
    local_values: Record<string, unknown>;
    server_values: Record<string, unknown>;
    resolution: 'pending' | 'keep_local' | 'keep_server' | 'merged';
    detected_at: string;
}

export class HarvestDB extends Dexie {
    bucket_queue!: Table<QueuedBucket, string>;
    message_queue!: Table<QueuedMessage, string>;
    user_cache!: Table<CachedUser, string>;
    settings_cache!: Table<CachedSettings, string>;
    runners_cache!: Table<unknown, string>;
    dead_letter_queue!: Table<DeadLetterItem, string>;
    sync_queue!: Table<QueuedSyncItem, string>;
    sync_meta!: Table<SyncMeta, string>;
    sync_conflicts!: Table<StoredConflict, string>;

    constructor() {
        super('HarvestProDB');
        this.version(3).stores({
            bucket_queue: 'id, picker_id, orchard_id, synced',
            message_queue: 'id, recipient_id, synced',
            user_cache: 'id',
            settings_cache: 'id',
            runners_cache: 'id'
        });
        // v4: compound indexes for high-volume queries (450+ pickers, ~20k+ buckets)
        this.version(4).stores({
            bucket_queue: 'id, picker_id, orchard_id, synced, [orchard_id+synced], [picker_id+synced], timestamp',
            message_queue: 'id, recipient_id, synced, [recipient_id+synced], timestamp',
            user_cache: 'id',
            settings_cache: 'id',
            runners_cache: 'id'
        });
        // v5: persistent Dead Letter Queue for failed sync items
        this.version(5).stores({
            bucket_queue: 'id, picker_id, orchard_id, synced, [orchard_id+synced], [picker_id+synced], timestamp',
            message_queue: 'id, recipient_id, synced, [recipient_id+synced], timestamp',
            user_cache: 'id',
            settings_cache: 'id',
            runners_cache: 'id',
            dead_letter_queue: 'id, type, timestamp, movedAt'
        });
        // v6: Migrate sync queue + conflicts from localStorage to IndexedDB
        // (Audit fix: eliminates 5MB limit, async I/O, no UI thread blocking)
        this.version(6).stores({
            bucket_queue: 'id, picker_id, orchard_id, synced, [orchard_id+synced], [picker_id+synced], timestamp',
            message_queue: 'id, recipient_id, synced, [recipient_id+synced], timestamp',
            user_cache: 'id',
            settings_cache: 'id',
            runners_cache: 'id',
            dead_letter_queue: 'id, type, timestamp, movedAt',
            sync_queue: 'id, type, timestamp, retryCount',
            sync_meta: 'id',
            sync_conflicts: 'id, table, record_id, resolution, detected_at'
        });
        // 🔧 V17: Add scanned_by to bucket_queue index + recovery table (V11)
        this.version(7).stores({
            bucket_queue: 'id, picker_id, orchard_id, synced, scanned_by, [orchard_id+synced], [picker_id+synced], timestamp',
            message_queue: 'id, recipient_id, synced, [recipient_id+synced], timestamp',
            user_cache: 'id',
            settings_cache: 'id',
            runners_cache: 'id',
            dead_letter_queue: 'id, type, timestamp, movedAt',
            sync_queue: 'id, type, timestamp, retryCount',
            sync_meta: 'id',
            sync_conflicts: 'id, table, record_id, resolution, detected_at',
            recovery: 'id, timestamp'
        });
    }
}

export const db = new HarvestDB();

