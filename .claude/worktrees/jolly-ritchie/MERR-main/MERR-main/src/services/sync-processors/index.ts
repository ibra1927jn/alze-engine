/**
 * Sync Processors — Strategy Pattern barrel export.
 *
 * Each processor handles a specific domain's sync logic,
 * keeping the main sync.service.ts as a pure orchestrator.
 */
export { processContract } from './contract.processor';
export { processTransport } from './transport.processor';
export { processTimesheet } from './timesheet.processor';
export { processAttendance } from './attendance.processor';

export type {
    ScanPayload,
    MessagePayload,
    AttendancePayload,
    ContractPayload,
    TransportPayload,
    TimesheetPayload,
    SyncPayload,
    PendingItem,
} from './types';
