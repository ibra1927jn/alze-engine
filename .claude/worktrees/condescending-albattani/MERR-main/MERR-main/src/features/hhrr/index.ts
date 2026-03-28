/**
 * features/hhrr — Domain barrel export
 * 
 * Human resources: contracts, visas, crew management.
 */

// Services (standalone exported functions)
export {
    fetchHRSummary,
    fetchEmployees,
    fetchContracts,
    createContract,
    updateContract,
    fetchPayroll,
    fetchComplianceAlerts,
} from '@/services/hhrr.service';

// Types
export type {
    Employee,
    Contract,
    PayrollEntry,
    ComplianceAlert,
    HRSummary,
} from '@/services/hhrr.service';
