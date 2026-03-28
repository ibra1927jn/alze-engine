/**
 * HHRR Service — Human Resources Department
 * Handles employee management, contracts, payroll, and compliance for HR_ADMIN role
 *
 * Architecture:
 *   READS  → Via domain repositories (attendance, user, contract)
 *   WRITES → Via syncService.addToQueue() for offline-first
 *   Conflict Resolution: Last-write-wins (documented decision)
 */
import { supabase } from '@/services/supabase';
import { storeSyncRepository } from '@/repositories/storeSync.repository';
import { syncService } from '@/services/sync.service';
import { logger } from '@/utils/logger';
import { todayNZST, nowNZST } from '@/utils/nzst';
import { userRepository2 } from '@/repositories/user.repository';
import { contractRepository2 } from '@/repositories/contract.repository';
import { attendanceRepository } from '@/repositories/attendance.repository';

// ── Types ──────────────────────────────────────
export interface Employee {
    id: string;
    full_name: string;
    email: string;
    role: string;
    status: 'active' | 'on_leave' | 'terminated' | 'pending';
    contract_type: 'permanent' | 'seasonal' | 'casual';
    contract_start: string;
    contract_end?: string;
    hourly_rate: number;
    visa_status: 'citizen' | 'resident' | 'work_visa' | 'expired';
    visa_expiry?: string;
    phone?: string;
    emergency_contact?: string;
    hire_date: string;
    orchard_id?: string;
    team_id?: string;
    documents_count?: number;
}

export interface Contract {
    id: string;
    employee_id: string;
    employee_name: string;
    type: 'permanent' | 'seasonal' | 'casual';
    status: 'active' | 'expiring' | 'expired' | 'draft' | 'terminated';
    start_date: string;
    end_date?: string;
    hourly_rate: number;
    notes?: string;
    created_at: string;
    updated_at: string;
}

export interface PayrollEntry {
    id: string;
    employee_id: string;
    employee_name: string;
    role: string;
    hours_worked: number;
    buckets_picked: number;
    hourly_earnings: number;
    piece_earnings: number;
    total_pay: number;
    wage_shield_applied: boolean;
    period_start: string;
    period_end: string;
}

export interface ComplianceAlert {
    id: string;
    type: 'visa_expiry' | 'wage_violation' | 'missing_document' | 'contract_expiry';
    severity: 'low' | 'medium' | 'high' | 'critical';
    employee_id: string;
    employee_name: string;
    message: string;
    created_at: string;
    resolved: boolean;
}

export interface HRSummary {
    activeWorkers: number;
    pendingContracts: number;
    payrollThisWeek: number;
    complianceAlerts: number;
}

// ── Service Functions ──────────────────────────

export async function fetchHRSummary(orchardId?: string): Promise<HRSummary> {
    try {
        const activeWorkers = await userRepository2.getActiveCount(orchardId);
        const pending = await contractRepository2.getPending(orchardId);
        const alerts = await fetchComplianceAlerts(orchardId);

        const sinceDate = new Date(Date.now() - 7 * 86400000).toISOString().split('T')[0];
        const attendance = await attendanceRepository.getHoursSummary(orchardId, sinceDate);

        let totalHours = 0;
        attendance.forEach(a => {
            if (a.check_in_time && a.check_out_time) {
                const hrs = (new Date(a.check_out_time).getTime() - new Date(a.check_in_time).getTime()) / 3600000;
                totalHours += Math.max(0, Math.min(hrs, 12));
            }
        });

        return {
            activeWorkers,
            pendingContracts: pending.length,
            payrollThisWeek: totalHours * 23.50,
            complianceAlerts: alerts.length,
        };
    } catch (error) {
        logger.error('[HHRR] Error fetching HR summary:', error);
        return { activeWorkers: 0, pendingContracts: 0, payrollThisWeek: 0, complianceAlerts: 0 };
    }
}

export async function fetchEmployees(orchardId?: string): Promise<Employee[]> {
    try {
        const data = await userRepository2.getAll(orchardId);
        const userIds = data.map(u => u.id);
        const contracts = await contractRepository2.getByEmployeeIds(userIds);

        return data.map(user => {
            const contract = contracts.find(c => c.employee_id === user.id);
            return {
                id: user.id,
                full_name: user.full_name || 'Unknown',
                email: user.email || '',
                role: user.role || 'picker',
                status: user.is_active ? 'active' as const : 'terminated' as const,
                contract_type: (contract?.type || 'seasonal') as 'permanent' | 'seasonal' | 'casual',
                contract_start: contract?.start_date || user.created_at || new Date().toISOString(),
                contract_end: contract?.end_date || undefined,
                hourly_rate: contract?.hourly_rate || 23.50,
                visa_status: 'citizen' as const,
                hire_date: user.created_at || new Date().toISOString(),
                orchard_id: user.orchard_id,
            };
        });
    } catch (error) {
        logger.error('[HHRR] Error fetching employees:', error);
        return [];
    }
}

export async function fetchContracts(orchardId?: string): Promise<Contract[]> {
    try {
        const data = await contractRepository2.getAll(orchardId);
        const employeeIds = [...new Set(data.map(c => c.employee_id))];
        const names = await userRepository2.getNamesByIds(employeeIds);

        return data.map(c => ({
            id: c.id,
            employee_id: c.employee_id,
            employee_name: names[c.employee_id] || 'Unknown',
            type: c.type,
            status: c.status,
            start_date: c.start_date,
            end_date: c.end_date || undefined,
            hourly_rate: c.hourly_rate,
            notes: c.notes || undefined,
            created_at: c.created_at,
            updated_at: c.updated_at,
        }));
    } catch (error) {
        logger.error('[HHRR] Error fetching contracts:', error);
        return [];
    }
}

export async function createContract(contract: {
    employee_id: string;
    orchard_id: string;
    type: 'permanent' | 'seasonal' | 'casual';
    start_date: string;
    end_date?: string;
    hourly_rate: number;
    notes?: string;
}): Promise<string> {
    return syncService.addToQueue('CONTRACT', { action: 'create', ...contract });
}

export async function updateContract(contractId: string, updates: {
    status?: string;
    end_date?: string;
    hourly_rate?: number;
    notes?: string;
}, currentUpdatedAt?: string): Promise<string> {
    return syncService.addToQueue('CONTRACT', { action: 'update', contractId, ...updates }, currentUpdatedAt);
}

export async function fetchPayroll(orchardId?: string): Promise<PayrollEntry[]> {
    try {
        const today = todayNZST();
        const sevenDaysAgo = new Date(new Date(today).getTime() - 7 * 86400000);
        const periodStart = sevenDaysAgo.toISOString().split('T')[0];
        const periodEnd = nowNZST();

        const employees = await fetchEmployees(orchardId);
        const activeEmployees = employees.filter(e => e.status === 'active');

        // Bucket counts via repo
        const bucketsData = await storeSyncRepository.getBucketCounts(orchardId, periodStart);

        const bucketCounts: Record<string, number> = {};
        (bucketsData || []).forEach(b => {
            bucketCounts[b.picker_id] = (bucketCounts[b.picker_id] || 0) + 1;
        });

        const attendance = await attendanceRepository.getHoursSummary(orchardId, periodStart.split('T')[0]);
        const hoursByPicker: Record<string, number> = {};
        attendance.forEach(a => {
            if (a.check_in_time && a.check_out_time) {
                const hrs = (new Date(a.check_out_time).getTime() - new Date(a.check_in_time).getTime()) / 3600000;
                if (hrs > 12) {
                    logger.warn(`[HHRR] Capping ${a.picker_id} hours from ${hrs.toFixed(1)} to 12 — review attendance record`);
                }
                hoursByPicker[a.picker_id] = (hoursByPicker[a.picker_id] || 0) + Math.max(0, Math.min(hrs, 12));
            }
        });

        let pieceRateValue = 6.50;
        if (orchardId) {
            const { data: settings } = await supabase
                .from('harvest_settings').select('piece_rate').eq('orchard_id', orchardId).single();
            if (settings?.piece_rate) pieceRateValue = settings.piece_rate;
        }

        return activeEmployees.map(emp => {
            const hours = hoursByPicker[emp.id] || 0;
            const bucketsCount = bucketCounts[emp.id] || 0;
            const hourlyEarnings = hours * emp.hourly_rate;
            const pieceEarnings = bucketsCount * pieceRateValue;
            const totalPay = Math.max(hourlyEarnings, pieceEarnings);

            return {
                id: `payroll-${emp.id}`,
                employee_id: emp.id,
                employee_name: emp.full_name,
                role: emp.role,
                hours_worked: Math.round(hours * 100) / 100,
                buckets_picked: bucketsCount,
                hourly_earnings: Math.round(hourlyEarnings * 100) / 100,
                piece_earnings: Math.round(pieceEarnings * 100) / 100,
                total_pay: Math.round(totalPay * 100) / 100,
                wage_shield_applied: pieceEarnings < hourlyEarnings,
                period_start: periodStart,
                period_end: periodEnd,
            };
        });
    } catch (error) {
        logger.error('[HHRR] Error generating payroll:', error);
        return [];
    }
}

export async function fetchComplianceAlerts(orchardId?: string): Promise<ComplianceAlert[]> {
    const alerts: ComplianceAlert[] = [];

    try {
        const thirtyDaysFromNow = new Date(Date.now() + 30 * 86400000).toISOString().split('T')[0];
        const today = new Date().toISOString().split('T')[0];

        const expiringContracts = await contractRepository2.getExpiringSoon(orchardId, today, thirtyDaysFromNow);

        if (expiringContracts.length > 0) {
            const ids = expiringContracts.map(c => c.employee_id);
            const names = await userRepository2.getNamesByIds(ids);

            expiringContracts.forEach(c => {
                const daysUntilExpiry = Math.floor(
                    (new Date(c.end_date).getTime() - Date.now()) / 86400000
                );
                alerts.push({
                    id: `contract-${c.id}`,
                    type: 'contract_expiry',
                    severity: daysUntilExpiry < 7 ? 'critical' : daysUntilExpiry < 14 ? 'high' : 'medium',
                    employee_id: c.employee_id,
                    employee_name: names[c.employee_id] || 'Unknown',
                    message: `${c.type} contract expires in ${daysUntilExpiry} days (${c.end_date})`,
                    created_at: new Date().toISOString(),
                    resolved: false,
                });
            });
        }

        const expired = await contractRepository2.getExpiredButActive(orchardId);
        if (expired.length > 0) {
            const ids = expired.map(c => c.employee_id);
            const names = await userRepository2.getNamesByIds(ids);

            expired.forEach(c => {
                alerts.push({
                    id: `expired-${c.id}`,
                    type: 'contract_expiry',
                    severity: 'critical',
                    employee_id: c.employee_id,
                    employee_name: names[c.employee_id] || 'Unknown',
                    message: 'Contract has expired but is still marked as active',
                    created_at: new Date().toISOString(),
                    resolved: false,
                });
            });
        }
    } catch (error) {
        logger.error('[HHRR] Error fetching compliance alerts:', error);
    }

    return alerts;
}
