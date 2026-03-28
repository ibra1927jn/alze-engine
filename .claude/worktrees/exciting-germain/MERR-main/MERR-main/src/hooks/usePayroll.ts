/**
 * usePayroll — Data loading for the Payroll page
 */
import { useState, useEffect } from 'react';
import { useHarvestStore } from '@/stores/useHarvestStore';
import { payrollService, PayrollResult } from '@/services/payroll.service';
import { logger } from '@/utils/logger';

export function usePayroll() {
    const orchardId = useHarvestStore((s) => s.orchard?.id);
    const [payrollData, setPayrollData] = useState<PayrollResult | null>(null);
    const [isLoading, setIsLoading] = useState(true);

    useEffect(() => {
        const loadPayroll = async () => {
            if (!orchardId) { setIsLoading(false); return; }
            try {
                setIsLoading(true);
                const data = await payrollService.calculateToday(orchardId);
                setPayrollData(data);
            } catch (err) {
                logger.warn('[Payroll] Failed to load payroll data:', err);
            } finally {
                setIsLoading(false);
            }
        };
        loadPayroll();
    }, [orchardId]);

    const summary = payrollData?.summary || { total_buckets: 0, total_hours: 0, total_piece_rate_earnings: 0, total_top_up: 0, total_earnings: 0 };
    const compliance = payrollData?.compliance || { workers_below_minimum: 0, workers_total: 0, compliance_rate: 100 };
    const pickers = payrollData?.picker_breakdown || [];
    const settings = payrollData?.settings || { bucket_rate: 0, min_wage_rate: 23.50 };

    return { orchardId, isLoading, summary, compliance, pickers, settings };
}
