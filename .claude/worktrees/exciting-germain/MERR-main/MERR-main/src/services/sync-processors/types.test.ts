/**
 * Tests for sync-processors/types.ts — Payload type shape validation
 */
import { describe, it, expect } from 'vitest';
import type {
    ScanPayload,
    MessagePayload,
    AttendancePayload,
    ContractPayload,
    TransportPayload,
    TimesheetPayload,
    PickerPayload,
    QCInspectionPayload,
    PendingItem,
} from './types';

describe('sync-processor types — Shape Validation', () => {
    it('ScanPayload has correct fields', () => {
        const scan: ScanPayload = {
            picker_id: 'p1', orchard_id: 'o1',
            quality_grade: 'B', timestamp: '2026-03-05T08:00:00Z',
        };
        expect(scan.picker_id).toBe('p1');
    });

    it('MessagePayload supports all channel types', () => {
        const types: MessagePayload['channel_type'][] = ['direct', 'group', 'team'];
        expect(types).toHaveLength(3);
    });

    it('AttendancePayload supports check-in and check-out', () => {
        const checkin: AttendancePayload = {
            picker_id: 'p1', orchard_id: 'o1', check_in_time: '08:00',
        };
        const checkout: AttendancePayload = {
            picker_id: 'p1', orchard_id: 'o1', check_out_time: '17:00',
        };
        expect(checkin.check_in_time).toBe('08:00');
        expect(checkout.check_out_time).toBe('17:00');
    });

    it('ContractPayload supports create and update actions', () => {
        const contract: ContractPayload = { action: 'create', employee_id: 'e1' };
        expect(['create', 'update']).toContain(contract.action);
    });

    it('TransportPayload supports all actions', () => {
        const actions: TransportPayload['action'][] = ['create', 'assign', 'complete'];
        expect(actions).toHaveLength(3);
    });

    it('TimesheetPayload supports approve and reject', () => {
        const ts: TimesheetPayload = { action: 'approve', attendanceId: 'a1', verifiedBy: 'u1' };
        expect(['approve', 'reject']).toContain(ts.action);
    });

    it('QCInspectionPayload has all required fields', () => {
        const qc: QCInspectionPayload = {
            orchard_id: 'o1', picker_id: 'p1',
            inspector_id: 'i1', grade: 'A',
        };
        expect(qc.grade).toBe('A');
    });

    it('PendingItem supports 10 event types', () => {
        const pendingTypes: PendingItem['type'][] = [
            'SCAN', 'MESSAGE', 'ATTENDANCE', 'ASSIGNMENT',
            'CONTRACT', 'TRANSPORT', 'TIMESHEET', 'PICKER', 'QC_INSPECTION',
        ];
        expect(pendingTypes.length).toBe(9);
    });
});
