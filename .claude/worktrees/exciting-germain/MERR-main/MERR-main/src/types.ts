export enum Role {
  MANAGER = 'manager',
  TEAM_LEADER = 'team_leader',
  RUNNER = 'runner',
  QC_INSPECTOR = 'qc_inspector',
  PAYROLL_ADMIN = 'payroll_admin',
  ADMIN = 'admin',
  HR_ADMIN = 'hr_admin',
  LOGISTICS = 'logistics'
}

// Navigation Tab Types
export type Tab = 'dashboard' | 'teams' | 'logistics' | 'messaging' | 'map' | 'settings' | 'timesheet' | 'analytics' | 'reports' | 'insights' | 'more';

export interface AppUser {
  id: string;
  email: string;
  full_name: string;
  role: Role; // Usa el Enum Role
  is_active: boolean;
  orchard_id?: string;
  team_id?: string;
  created_at?: string;
  updated_at?: string;
}

export interface Picker {
  id: string; // UUID from DB
  picker_id: string; // "402" - ID for Sticker/QR
  name: string; // "Liam O."
  avatar: string; // Initials or URL
  current_row: number;
  total_buckets_today: number;
  hours: number;
  status: 'active' | 'break' | 'on_break' | 'issue' | 'inactive' | 'suspended' | 'archived';
  safety_verified: boolean; // Was onboarded
  qcStatus: number[]; // 0 = bad, 1 = good, 2 = warning
  harness_id?: string;
  team_leader_id?: string;
  orchard_id?: string;
  role?: string; // Added for Manager UI filtering

  // 🔴 FASE 9: Attendance cache for offline-first validation
  checked_in_today?: boolean; // Whether picker checked in today (cached from daily_attendance)
  check_in_time?: string; // ISO timestamp of check-in
  archived_at?: string; // Timestamp when picker was archived (soft delete)
}

export interface Bin {
  id: string; // "#4092"
  status: 'empty' | 'in-progress' | 'full' | 'collected';
  fillPercentage: number;
  type: 'Standard' | 'Export' | 'Process';
  assignedRunner?: string;
  row?: string;
  sunExposureStart?: number; // Timestamp
  bin_code?: string;
}

export interface Notification {
  id: string;
  from: string;
  message: string;
  priority: 'normal' | 'high';
  timestamp: string;
  read: boolean;
}

export interface HarvestSettings {
  min_wage_rate: number;
  piece_rate: number;
  min_buckets_per_hour: number;
  target_tons: number;
  variety?: string;
}

export interface HarvestState {
  currentUser: {
    name: string;
    role: Role | null;
    avatarUrl?: string;
    id?: string;
  };
  crew: Picker[];
  bins: Bin[];
  notifications: Notification[];
  stats: {
    totalBuckets: number;
    payEstimate: number; // In thousands, e.g. 4.2
    tons: number;
    velocity: number; // bkt/hr
    goalVelocity: number;
  };
  settings?: HarvestSettings;
  orchard?: {
    id: string;
    name?: string;
    total_rows?: number;
  };
  bucketRecords: BucketRecord[]; // Stream for HeatMap
  selectedBinId?: string;
}

// View Mapped Interface
export interface PickerPerformance {
  picker_id: string;
  picker_name: string;
  harness_id: string;
  team_id?: string;
  orchard_id: string;
  total_buckets: number;
  first_scan: string;
  last_scan: string;
  hours_worked: number;
  buckets_per_hour: number;
  status_shield: 'safe' | 'warning' | 'critical';
}

export interface BucketEvent {
  id?: string;
  picker_id: string;
  orchard_id?: string;
  scanned_by?: string; // The user (Runner/Admin) who scanned
  device_id?: string;
  row_number?: number;
  quality_grade: 'A' | 'B' | 'C' | 'reject';
  scanned_at?: string; // ISO timestamp
  bin_id?: string;
}

export type MessagePriority = 'normal' | 'high' | 'urgent';

export interface Message {
  id: string;
  channel_type: 'direct' | 'team' | 'broadcast';
  sender_id: string;
  recipient_id?: string;
  group_id?: string;
  content: string;
  created_at: string;
  read_by: string[];
  priority: MessagePriority;
  orchard_id?: string;
}

export interface Broadcast {
  id: string;
  orchard_id: string;
  sender_id: string;
  title: string;
  content: string;
  priority: MessagePriority;
  target_roles: Role[];
  acknowledged_by: string[];
  created_at: string;
}

export interface BucketRecord {
  id: string;
  timestamp: string;
  picker_id: string;
  bin_id: string;
  // Extended props for HeatMap
  coords?: { lat: number; lng: number };
  bucket_count?: number;
  scanned_at?: string; // Added for Manager.tsx compatibility
  row_number?: number;
  // Runtime props from Supabase joins
  created_at?: string;
  picker_name?: string;
  quality_grade?: 'A' | 'B' | 'C' | 'reject';
  orchard_id?: string;
  // Soft delete & optimistic locking (used by delta sync)
  deleted_at?: string | null;
  updated_at?: string;
  version?: number;
}

export type PickerStatus = 'active' | 'break' | 'on_break' | 'issue' | 'inactive' | 'suspended' | 'archived';

export interface Alert {
  id: string;
  type: 'compliance' | 'performance' | 'system';
  message: string;
  severity: 'low' | 'medium' | 'high';
  title?: string; // Add title as optional or required based on usage
  description?: string;
  timestamp: string;
}

export interface QualityInspection {
  id: string;
  picker_id: string;
  quality_grade: 'A' | 'B' | 'C' | 'reject';
  created_at: string;
  inspector_id: string;
  notes?: string;
  photo_url?: string;
}

// === CONSTANTS ===
export const MINIMUM_WAGE = 23.50; // NZD Minimum Wage
export const PIECE_RATE = 6.50;    // Per bucket
export const MAX_BUCKETS_PER_BIN = 72;
export const DEFAULT_START_TIME = '07:00';

export interface RowAssignment {
  id: string;
  row_number: number;
  side: 'north' | 'south';
  assigned_pickers: string[];
  completion_percentage: number;
}

// === ORCHARD BLOCKS (Tactical Map) ===
export type BlockStatus = 'idle' | 'active' | 'complete' | 'alert';

export interface OrchardBlock {
  id: string;
  name: string;           // "Block A", "Block B"
  totalRows: number;       // 20
  startRow: number;        // First row number in this block (1-indexed)
  colorCode: string;       // Hex for visual distinction
  status: BlockStatus;
  /** Maps row number → cherry variety name. Each row has its own variety. */
  rowVarieties: Record<number, string>;
}

export interface HarvestPrediction {
  estimatedCompletionTime: string;
  probabilityOfSuccess: number;
  predictedFinalTons: number;
  riskFactors: string[];
  recommendations: string[];
  confidence: number;
  // Legacy support if needed
  predicted_tons?: number;
  weather_impact?: string;
  recommended_action?: string;
}
export interface PredictionParams {
  currentTons: number;
  targetTons: number;
  velocity: number; // buckets per hour
  hoursRemaining: number;
  crewSize: number;
  weatherConditions?: string;
  blockProgress?: number;
}

export interface AttendanceRecord {
  id?: string;
  picker_id: string;
  status: 'present' | 'absent' | 'sick' | 'late' | 'left_early';
  check_in_time?: string;
  check_out_time?: string;
  orchard_id?: string;
  date?: string;
  verified_by?: string;
}

export interface LiveProductionStat {
  picker_id: string;
  picker_name: string;
  total_buckets: number;
  hours_worked: number;
  buckets_per_hour: number;
  last_scan_time?: string;
  efficiency_rating: number; // 0-100%
}