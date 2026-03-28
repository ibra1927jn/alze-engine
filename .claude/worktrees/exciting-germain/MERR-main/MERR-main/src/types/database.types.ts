/**
 * HarvestPro NZ — Database Types
 * ================================
 * Schema-aligned types for all Supabase tables.
 * Source of truth: supabase/schema_v1_consolidated.sql + migrations.
 *
 * When Supabase CLI is available, regenerate with:
 *   npx supabase gen types typescript --project-id <PROJECT_ID> > src/types/database.types.ts
 */

// ============================================
// JSON & PRIMITIVE TYPES
// ============================================

export type Json =
    | string
    | number
    | boolean
    | null
    | { [key: string]: Json | undefined }
    | Json[];

/** UUID type alias for semantic clarity */
export type UUID = string;

/** ISO 8601 timestamp string */
export type Timestamp = string;

/** Quality grade enum */
export type QualityGrade = 'A' | 'B' | 'C' | 'reject';

/** Database-level quality grades (superset, includes legacy) */
export type DbQualityGrade = 'good' | 'warning' | 'bad' | 'A' | 'B' | 'C' | 'reject';

/** User role enum (DB-level, includes qc_inspector) */
export type UserRole = 'manager' | 'team_leader' | 'runner' | 'qc_inspector' | 'payroll_admin' | 'admin' | 'hr_admin' | 'logistics';

// ============================================
// DATABASE INTERFACE (Supabase-compatible)
// ============================================

export interface Database {
    public: {
        Tables: {
            orchards: {
                Row: {
                    id: string;
                    code: string | null;
                    name: string;
                    location: string | null;
                    total_blocks: number;
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    code?: string | null;
                    name: string;
                    location?: string | null;
                    total_blocks?: number;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    code?: string | null;
                    name?: string;
                    location?: string | null;
                    total_blocks?: number;
                    created_at?: string;
                };
            };

            users: {
                Row: {
                    id: string;
                    email: string | null;
                    full_name: string | null;
                    role: UserRole;
                    orchard_id: string | null;
                    team_id: string | null;
                    is_active: boolean;
                    created_at: string;
                };
                Insert: {
                    id: string;
                    email?: string | null;
                    full_name?: string | null;
                    role?: UserRole;
                    orchard_id?: string | null;
                    team_id?: string | null;
                    is_active?: boolean;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    email?: string | null;
                    full_name?: string | null;
                    role?: UserRole;
                    orchard_id?: string | null;
                    team_id?: string | null;
                    is_active?: boolean;
                    created_at?: string;
                };
            };

            pickers: {
                Row: {
                    id: string;
                    picker_id: string;
                    name: string;
                    orchard_id: string | null;
                    team_leader_id: string | null;
                    safety_verified: boolean;
                    total_buckets_today: number;
                    current_row: number;
                    status: string;
                    created_at: string;
                    archived_at: string | null;
                };
                Insert: {
                    id?: string;
                    picker_id: string;
                    name: string;
                    orchard_id?: string | null;
                    team_leader_id?: string | null;
                    safety_verified?: boolean;
                    total_buckets_today?: number;
                    current_row?: number;
                    status?: string;
                    created_at?: string;
                    archived_at?: string | null;
                };
                Update: {
                    id?: string;
                    picker_id?: string;
                    name?: string;
                    orchard_id?: string | null;
                    team_leader_id?: string | null;
                    safety_verified?: boolean;
                    total_buckets_today?: number;
                    current_row?: number;
                    status?: string;
                    created_at?: string;
                    archived_at?: string | null;
                };
            };

            day_setups: {
                Row: {
                    id: string;
                    orchard_id: string | null;
                    date: string;
                    variety: string | null;
                    target_tons: number | null;
                    piece_rate: number;
                    min_wage_rate: number;
                    start_time: string | null;
                    created_by: string | null;
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    orchard_id?: string | null;
                    date?: string;
                    variety?: string | null;
                    target_tons?: number | null;
                    piece_rate?: number;
                    min_wage_rate?: number;
                    start_time?: string | null;
                    created_by?: string | null;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    orchard_id?: string | null;
                    date?: string;
                    variety?: string | null;
                    target_tons?: number | null;
                    piece_rate?: number;
                    min_wage_rate?: number;
                    start_time?: string | null;
                    created_by?: string | null;
                    created_at?: string;
                };
            };

            bucket_records: {
                Row: {
                    id: string;
                    orchard_id: string | null;
                    picker_id: string | null;
                    bin_id: string | null;
                    scanned_by: string | null;
                    scanned_at: string;
                    coords: Json | null;
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    orchard_id?: string | null;
                    picker_id?: string | null;
                    bin_id?: string | null;
                    scanned_by?: string | null;
                    scanned_at?: string;
                    coords?: Json | null;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    orchard_id?: string | null;
                    picker_id?: string | null;
                    bin_id?: string | null;
                    scanned_by?: string | null;
                    scanned_at?: string;
                    coords?: Json | null;
                    created_at?: string;
                };
            };

            bins: {
                Row: {
                    id: string;
                    orchard_id: string | null;
                    bin_code: string | null;
                    status: 'empty' | 'partial' | 'full' | 'collected';
                    variety: string | null;
                    location: Json | null;
                    movement_history: Json[];
                    filled_at: string | null;
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    orchard_id?: string | null;
                    bin_code?: string | null;
                    status?: 'empty' | 'partial' | 'full' | 'collected';
                    variety?: string | null;
                    location?: Json | null;
                    movement_history?: Json[];
                    filled_at?: string | null;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    orchard_id?: string | null;
                    bin_code?: string | null;
                    status?: 'empty' | 'partial' | 'full' | 'collected';
                    variety?: string | null;
                    location?: Json | null;
                    movement_history?: Json[];
                    filled_at?: string | null;
                    created_at?: string;
                };
            };

            quality_inspections: {
                Row: {
                    id: string;
                    bucket_id: string | null;
                    picker_id: string | null;
                    inspector_id: string | null;
                    quality_grade: DbQualityGrade | null;
                    notes: string | null;
                    photo_url: string | null;
                    coords: Json | null;
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    bucket_id?: string | null;
                    picker_id?: string | null;
                    inspector_id?: string | null;
                    quality_grade?: DbQualityGrade | null;
                    notes?: string | null;
                    photo_url?: string | null;
                    coords?: Json | null;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    bucket_id?: string | null;
                    picker_id?: string | null;
                    inspector_id?: string | null;
                    quality_grade?: DbQualityGrade | null;
                    notes?: string | null;
                    photo_url?: string | null;
                    coords?: Json | null;
                    created_at?: string;
                };
            };

            conversations: {
                Row: {
                    id: string;
                    type: 'direct' | 'group' | 'broadcast';
                    name: string | null;
                    participant_ids: string[];
                    created_by: string | null;
                    created_at: string;
                    updated_at: string;
                };
                Insert: {
                    id?: string;
                    type: 'direct' | 'group' | 'broadcast';
                    name?: string | null;
                    participant_ids?: string[];
                    created_by?: string | null;
                    created_at?: string;
                    updated_at?: string;
                };
                Update: {
                    id?: string;
                    type?: 'direct' | 'group' | 'broadcast';
                    name?: string | null;
                    participant_ids?: string[];
                    created_by?: string | null;
                    created_at?: string;
                    updated_at?: string;
                };
            };

            chat_messages: {
                Row: {
                    id: string;
                    conversation_id: string;
                    sender_id: string;
                    content: string;
                    read_by: string[];
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    conversation_id: string;
                    sender_id: string;
                    content: string;
                    read_by?: string[];
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    conversation_id?: string;
                    sender_id?: string;
                    content?: string;
                    read_by?: string[];
                    created_at?: string;
                };
            };

            day_closures: {
                Row: {
                    id: string;
                    orchard_id: string;
                    date: string;
                    status: 'open' | 'closed';
                    closed_by: string | null;
                    closed_at: string | null;
                    total_buckets: number;
                    total_cost: number;
                    total_hours: number | null;
                    wage_violations: number;
                    created_at: string;
                    updated_at: string;
                };
                Insert: {
                    id?: string;
                    orchard_id: string;
                    date?: string;
                    status: 'open' | 'closed';
                    closed_by?: string | null;
                    closed_at?: string | null;
                    total_buckets?: number;
                    total_cost?: number;
                    total_hours?: number | null;
                    wage_violations?: number;
                    created_at?: string;
                    updated_at?: string;
                };
                Update: {
                    id?: string;
                    orchard_id?: string;
                    date?: string;
                    status?: 'open' | 'closed';
                    closed_by?: string | null;
                    closed_at?: string | null;
                    total_buckets?: number;
                    total_cost?: number;
                    total_hours?: number | null;
                    wage_violations?: number;
                    created_at?: string;
                    updated_at?: string;
                };
            };

            daily_attendance: {
                Row: {
                    id: string;
                    picker_id: string;
                    orchard_id: string;
                    date: string;
                    check_in_time: string | null;
                    check_out_time: string | null;
                    status: 'present' | 'absent' | 'sick' | 'late' | 'left_early';
                    verified_by: string | null;
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    picker_id: string;
                    orchard_id: string;
                    date?: string;
                    check_in_time?: string | null;
                    check_out_time?: string | null;
                    status?: 'present' | 'absent' | 'sick' | 'late' | 'left_early';
                    verified_by?: string | null;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    picker_id?: string;
                    orchard_id?: string;
                    date?: string;
                    check_in_time?: string | null;
                    check_out_time?: string | null;
                    status?: 'present' | 'absent' | 'sick' | 'late' | 'left_early';
                    verified_by?: string | null;
                    created_at?: string;
                };
            };

            audit_logs: {
                Row: {
                    id: string;
                    user_id: string | null;
                    user_email: string | null;
                    action: 'INSERT' | 'UPDATE' | 'DELETE' | 'CUSTOM';
                    table_name: string;
                    record_id: string | null;
                    old_values: Json | null;
                    new_values: Json | null;
                    ip_address: string | null;
                    user_agent: string | null;
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    user_id?: string | null;
                    user_email?: string | null;
                    action: 'INSERT' | 'UPDATE' | 'DELETE' | 'CUSTOM';
                    table_name: string;
                    record_id?: string | null;
                    old_values?: Json | null;
                    new_values?: Json | null;
                    ip_address?: string | null;
                    user_agent?: string | null;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    user_id?: string | null;
                    user_email?: string | null;
                    action?: 'INSERT' | 'UPDATE' | 'DELETE' | 'CUSTOM';
                    table_name?: string;
                    record_id?: string | null;
                    old_values?: Json | null;
                    new_values?: Json | null;
                    ip_address?: string | null;
                    user_agent?: string | null;
                    created_at?: string;
                };
            };

            login_attempts: {
                Row: {
                    id: string;
                    email: string;
                    ip_address: string | null;
                    attempt_time: string;
                    success: boolean;
                    user_agent: string | null;
                    failure_reason: string | null;
                };
                Insert: {
                    id?: string;
                    email: string;
                    ip_address?: string | null;
                    attempt_time?: string;
                    success?: boolean;
                    user_agent?: string | null;
                    failure_reason?: string | null;
                };
                Update: {
                    id?: string;
                    email?: string;
                    ip_address?: string | null;
                    attempt_time?: string;
                    success?: boolean;
                    user_agent?: string | null;
                    failure_reason?: string | null;
                };
            };

            account_locks: {
                Row: {
                    id: string;
                    user_id: string | null;
                    email: string;
                    locked_at: string;
                    locked_until: string;
                    locked_by_system: boolean;
                    unlock_reason: string | null;
                    unlocked_by: string | null;
                    unlocked_at: string | null;
                };
                Insert: {
                    id?: string;
                    user_id?: string | null;
                    email: string;
                    locked_at?: string;
                    locked_until: string;
                    locked_by_system?: boolean;
                    unlock_reason?: string | null;
                    unlocked_by?: string | null;
                    unlocked_at?: string | null;
                };
                Update: {
                    id?: string;
                    user_id?: string | null;
                    email?: string;
                    locked_at?: string;
                    locked_until?: string;
                    locked_by_system?: boolean;
                    unlock_reason?: string | null;
                    unlocked_by?: string | null;
                    unlocked_at?: string | null;
                };
            };

            // === NEW: Hierarchy Tables (Sprint 1) ===

            harvest_seasons: {
                Row: {
                    id: string;
                    orchard_id: string;
                    name: string;
                    start_date: string;
                    end_date: string | null;
                    status: 'planning' | 'active' | 'closed' | 'archived';
                    deleted_at: string | null;
                    created_at: string;
                    updated_at: string;
                };
                Insert: {
                    id?: string;
                    orchard_id: string;
                    name: string;
                    start_date: string;
                    end_date?: string | null;
                    status?: 'planning' | 'active' | 'closed' | 'archived';
                    deleted_at?: string | null;
                    created_at?: string;
                    updated_at?: string;
                };
                Update: {
                    id?: string;
                    orchard_id?: string;
                    name?: string;
                    start_date?: string;
                    end_date?: string | null;
                    status?: 'planning' | 'active' | 'closed' | 'archived';
                    deleted_at?: string | null;
                    created_at?: string;
                    updated_at?: string;
                };
            };

            orchard_blocks: {
                Row: {
                    id: string;
                    orchard_id: string;
                    season_id: string;
                    name: string;
                    total_rows: number;
                    start_row: number;
                    color_code: string | null;
                    status: 'idle' | 'active' | 'complete' | 'alert';
                    deleted_at: string | null;
                    created_at: string;
                    updated_at: string;
                };
                Insert: {
                    id?: string;
                    orchard_id: string;
                    season_id: string;
                    name: string;
                    total_rows?: number;
                    start_row?: number;
                    color_code?: string | null;
                    status?: 'idle' | 'active' | 'complete' | 'alert';
                    deleted_at?: string | null;
                    created_at?: string;
                    updated_at?: string;
                };
                Update: {
                    id?: string;
                    orchard_id?: string;
                    season_id?: string;
                    name?: string;
                    total_rows?: number;
                    start_row?: number;
                    color_code?: string | null;
                    status?: 'idle' | 'active' | 'complete' | 'alert';
                    deleted_at?: string | null;
                    created_at?: string;
                    updated_at?: string;
                };
            };

            block_rows: {
                Row: {
                    id: string;
                    block_id: string;
                    row_number: number;
                    variety: string | null;
                    target_buckets: number;
                    deleted_at: string | null;
                    created_at: string;
                };
                Insert: {
                    id?: string;
                    block_id: string;
                    row_number: number;
                    variety?: string | null;
                    target_buckets?: number;
                    deleted_at?: string | null;
                    created_at?: string;
                };
                Update: {
                    id?: string;
                    block_id?: string;
                    row_number?: number;
                    variety?: string | null;
                    target_buckets?: number;
                    deleted_at?: string | null;
                    created_at?: string;
                };
            };
        };

        Functions: {
            get_my_orchard_id: {
                Args: Record<string, never>;
                Returns: string;
            };
            is_manager_or_leader: {
                Args: Record<string, never>;
                Returns: boolean;
            };
            get_auth_role: {
                Args: Record<string, never>;
                Returns: string;
            };
            get_auth_orchard_id: {
                Args: Record<string, never>;
                Returns: string;
            };
            is_account_locked: {
                Args: { check_email: string };
                Returns: boolean;
            };
            unlock_account: {
                Args: { target_email: string; unlock_reason_text?: string };
                Returns: boolean;
            };
            get_record_audit_trail: {
                Args: { p_table_name: string; p_record_id: string };
                Returns: {
                    id: string;
                    action: string;
                    user_email: string;
                    old_values: Json;
                    new_values: Json;
                    created_at: string;
                }[];
            };
        };

        contracts: {
            Row: {
                id: string;
                employee_id: string;
                orchard_id: string;
                type: 'permanent' | 'seasonal' | 'casual';
                status: 'active' | 'expiring' | 'expired' | 'draft' | 'terminated';
                start_date: string;
                end_date: string | null;
                hourly_rate: number;
                notes: string | null;
                created_by: string | null;
                created_at: string;
                updated_at: string;
            };
            Insert: {
                id?: string;
                employee_id: string;
                orchard_id: string;
                type: 'permanent' | 'seasonal' | 'casual';
                status?: 'active' | 'expiring' | 'expired' | 'draft' | 'terminated';
                start_date: string;
                end_date?: string | null;
                hourly_rate?: number;
                notes?: string | null;
                created_by?: string | null;
                created_at?: string;
                updated_at?: string;
            };
            Update: {
                id?: string;
                employee_id?: string;
                orchard_id?: string;
                type?: 'permanent' | 'seasonal' | 'casual';
                status?: 'active' | 'expiring' | 'expired' | 'draft' | 'terminated';
                start_date?: string;
                end_date?: string | null;
                hourly_rate?: number;
                notes?: string | null;
                created_by?: string | null;
                created_at?: string;
                updated_at?: string;
            };
        };

        fleet_vehicles: {
            Row: {
                id: string;
                orchard_id: string;
                name: string;
                registration: string | null;
                zone: string | null;
                driver_id: string | null;
                driver_name: string | null;
                status: 'active' | 'idle' | 'maintenance' | 'offline';
                load_status: 'empty' | 'partial' | 'full';
                bins_loaded: number;
                max_capacity: number;
                fuel_level: number | null;
                last_service_date: string | null;
                next_service_date: string | null;
                wof_expiry: string | null;
                cof_expiry: string | null;
                created_at: string;
                updated_at: string;
            };
            Insert: {
                id?: string;
                orchard_id: string;
                name: string;
                registration?: string | null;
                zone?: string | null;
                driver_id?: string | null;
                driver_name?: string | null;
                status?: 'active' | 'idle' | 'maintenance' | 'offline';
                load_status?: 'empty' | 'partial' | 'full';
                bins_loaded?: number;
                max_capacity?: number;
                fuel_level?: number | null;
                last_service_date?: string | null;
                next_service_date?: string | null;
                wof_expiry?: string | null;
                cof_expiry?: string | null;
                created_at?: string;
                updated_at?: string;
            };
            Update: {
                id?: string;
                orchard_id?: string;
                name?: string;
                registration?: string | null;
                zone?: string | null;
                driver_id?: string | null;
                driver_name?: string | null;
                status?: 'active' | 'idle' | 'maintenance' | 'offline';
                load_status?: 'empty' | 'partial' | 'full';
                bins_loaded?: number;
                max_capacity?: number;
                fuel_level?: number | null;
                last_service_date?: string | null;
                next_service_date?: string | null;
                wof_expiry?: string | null;
                cof_expiry?: string | null;
                created_at?: string;
                updated_at?: string;
            };
        };

        transport_requests: {
            Row: {
                id: string;
                orchard_id: string;
                requested_by: string;
                requester_name: string;
                zone: string;
                bins_count: number;
                priority: 'normal' | 'high' | 'urgent';
                status: 'pending' | 'assigned' | 'in_progress' | 'completed' | 'cancelled';
                assigned_vehicle: string | null;
                assigned_by: string | null;
                notes: string | null;
                completed_at: string | null;
                created_at: string;
                updated_at: string;
            };
            Insert: {
                id?: string;
                orchard_id: string;
                requested_by: string;
                requester_name: string;
                zone: string;
                bins_count?: number;
                priority?: 'normal' | 'high' | 'urgent';
                status?: 'pending' | 'assigned' | 'in_progress' | 'completed' | 'cancelled';
                assigned_vehicle?: string | null;
                assigned_by?: string | null;
                notes?: string | null;
                completed_at?: string | null;
                created_at?: string;
                updated_at?: string;
            };
            Update: {
                id?: string;
                orchard_id?: string;
                requested_by?: string;
                requester_name?: string;
                zone?: string;
                bins_count?: number;
                priority?: 'normal' | 'high' | 'urgent';
                status?: 'pending' | 'assigned' | 'in_progress' | 'completed' | 'cancelled';
                assigned_vehicle?: string | null;
                assigned_by?: string | null;
                notes?: string | null;
                completed_at?: string | null;
                created_at?: string;
                updated_at?: string;
            };
        };
    };
}

// === Convenience Type Helpers ===

/** Extract Row type for a table: Tables<'users'> */
export type Tables<T extends keyof Database['public']['Tables']> =
    Database['public']['Tables'][T]['Row'];

/** Extract Insert type for a table: TablesInsert<'bucket_records'> */
export type TablesInsert<T extends keyof Database['public']['Tables']> =
    Database['public']['Tables'][T]['Insert'];

/** Extract Update type for a table: TablesUpdate<'pickers'> */
export type TablesUpdate<T extends keyof Database['public']['Tables']> =
    Database['public']['Tables'][T]['Update'];

// === Legacy Aliases (for backward compatibility) ===

export type SupabaseUser = Tables<'users'>;
export type SupabasePicker = Tables<'pickers'>;
export type SupabaseChatMessage = Tables<'chat_messages'> & {
    sender?: { name: string } | null;
};
export type SupabaseConversation = Tables<'conversations'>;
export type SupabaseAttendanceRecord = Tables<'daily_attendance'>;
export type SupabaseBucketRecord = Tables<'bucket_records'>;

/** Computed view type (not a direct table) — used by attendance & picker services */
export interface SupabasePerformanceStat {
    picker_id: string;
    total_buckets: number;
    avg_quality: QualityGrade | null;
    total_earnings?: number;
    last_scan?: string | null;
}

// ============================================
// TYPE GUARDS (Runtime Validation)
// ============================================

export function isSupabasePicker(item: unknown): item is SupabasePicker {
    if (!item || typeof item !== 'object') return false;
    const obj = item as Record<string, unknown>;
    return (
        typeof obj.id === 'string' &&
        typeof obj.name === 'string' &&
        typeof obj.picker_id === 'string' &&
        typeof obj.created_at === 'string'
    );
}

export function isSupabaseUser(item: unknown): item is SupabaseUser {
    if (!item || typeof item !== 'object') return false;
    const obj = item as Record<string, unknown>;
    return (
        typeof obj.id === 'string' &&
        typeof obj.role === 'string' &&
        typeof obj.created_at === 'string'
    );
}

export function isSupabaseChatMessage(item: unknown): item is SupabaseChatMessage {
    if (!item || typeof item !== 'object') return false;
    const obj = item as Record<string, unknown>;
    return (
        typeof obj.id === 'string' &&
        typeof obj.conversation_id === 'string' &&
        typeof obj.sender_id === 'string' &&
        typeof obj.content === 'string' &&
        typeof obj.created_at === 'string'
    );
}

export function isSupabaseAttendanceRecord(item: unknown): item is SupabaseAttendanceRecord {
    if (!item || typeof item !== 'object') return false;
    const obj = item as Record<string, unknown>;
    return (
        typeof obj.id === 'string' &&
        typeof obj.picker_id === 'string' &&
        typeof obj.orchard_id === 'string' &&
        typeof obj.created_at === 'string'
    );
}

export function isSupabasePickerArray(items: unknown): items is SupabasePicker[] {
    return Array.isArray(items) && items.every(isSupabasePicker);
}

export function isSupabaseUserArray(items: unknown): items is SupabaseUser[] {
    return Array.isArray(items) && items.every(isSupabaseUser);
}

export function isSupabaseChatMessageArray(items: unknown): items is SupabaseChatMessage[] {
    return Array.isArray(items) && items.every(isSupabaseChatMessage);
}
