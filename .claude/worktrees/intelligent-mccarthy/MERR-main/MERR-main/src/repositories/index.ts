/**
 * Typed Repository Layer — connects baseRepository to database.types.ts
 *
 * Each repository is a pre-typed instance of SupabaseRepository<T>
 * using the Row types from database.types.ts (source of truth).
 *
 * Services should import these instead of using supabase.from() directly.
 */
import { SupabaseRepository } from './baseRepository';
import type { Database } from '@/types/database.types';

// ── Table Row Type Aliases ──────────────────────────
type Tables = Database['public']['Tables'];
export type OrchardRow = Tables['orchards']['Row'];
export type UserRow = Tables['users']['Row'];
export type PickerRow = Tables['pickers']['Row'];
export type BucketRecordRow = Tables['bucket_records']['Row'];
export type BinRow = Tables['bins']['Row'];
export type DaySetupRow = Tables['day_setups']['Row'];
export type QualityInspectionRow = Tables['quality_inspections']['Row'];
export type ConversationRow = Tables['conversations']['Row'];
export type ChatMessageRow = Tables['chat_messages']['Row'];
export type DayClosureRow = Tables['day_closures']['Row'];
export type DailyAttendanceRow = Tables['daily_attendance']['Row'];
export type AuditLogRow = Tables['audit_logs']['Row'];
export type LoginAttemptRow = Tables['login_attempts']['Row'];
export type AccountLockRow = Tables['account_locks']['Row'];
export type HarvestSeasonRow = Tables['harvest_seasons']['Row'];
export type OrchardBlockRow = Tables['orchard_blocks']['Row'];
export type BlockRowRow = Tables['block_rows']['Row'];

// ── Public Extras (sibling to Tables in the `public` scope) ──
export type ContractRow = Database['public']['contracts']['Row'];
export type FleetVehicleRow = Database['public']['fleet_vehicles']['Row'];
export type TransportRequestRow = Database['public']['transport_requests']['Row'];

// ── Typed Repository Instances ──────────────────────
export const orchardRepo = new SupabaseRepository<OrchardRow>('orchards');
export const userRepo = new SupabaseRepository<UserRow>('users');
export const pickerRepo = new SupabaseRepository<PickerRow>('pickers');
export const bucketRecordRepo = new SupabaseRepository<BucketRecordRow>('bucket_records');
export const binRepo = new SupabaseRepository<BinRow>('bins');
export const daySetupRepo = new SupabaseRepository<DaySetupRow>('day_setups');
export const qcInspectionRepo = new SupabaseRepository<QualityInspectionRow>('quality_inspections');
export const conversationRepo = new SupabaseRepository<ConversationRow>('conversations');
export const chatMessageRepo = new SupabaseRepository<ChatMessageRow>('chat_messages');
export const dayClosureRepo = new SupabaseRepository<DayClosureRow>('day_closures');
export const attendanceRepo = new SupabaseRepository<DailyAttendanceRow>('daily_attendance');
export const auditLogRepo = new SupabaseRepository<AuditLogRow>('audit_logs');
export const loginAttemptRepo = new SupabaseRepository<LoginAttemptRow>('login_attempts');
export const accountLockRepo = new SupabaseRepository<AccountLockRow>('account_locks');
export const seasonRepo = new SupabaseRepository<HarvestSeasonRow>('harvest_seasons');
export const blockRepo = new SupabaseRepository<OrchardBlockRow>('orchard_blocks');
export const blockRowRepo = new SupabaseRepository<BlockRowRow>('block_rows');
export const contractRepo = new SupabaseRepository<ContractRow>('contracts');
export const fleetVehicleRepo = new SupabaseRepository<FleetVehicleRow>('fleet_vehicles');
export const transportRequestRepo = new SupabaseRepository<TransportRequestRow>('transport_requests');
