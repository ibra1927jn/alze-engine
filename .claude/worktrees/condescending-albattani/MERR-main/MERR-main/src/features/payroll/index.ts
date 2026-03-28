/**
 * features/payroll — Domain barrel export
 * 
 * Payroll calculations, compliance wage shield, piece rate management.
 */

// Services
export { payrollService } from '@/services/payroll.service';
export { complianceService } from '@/services/compliance.service';

// Schemas
export { HarvestSettingsSchema } from '@/lib/schemas';
export type { ValidatedSettings } from '@/lib/schemas';
