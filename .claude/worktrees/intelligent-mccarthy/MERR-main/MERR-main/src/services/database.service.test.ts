/**
 * Tests for database.service.ts — Facade validation
 */
import { describe, it, expect } from 'vitest';

vi.mock('./picker.service', () => ({ pickerService: { getPicker: vi.fn() } }));
vi.mock('./attendance.service', () => ({ attendanceService: { checkIn: vi.fn() } }));
vi.mock('./user.service', () => ({ userService: { getUser: vi.fn() } }));
vi.mock('./settings.service', () => ({ settingsService: { getSettings: vi.fn() } }));
vi.mock('./bin.service', () => ({ binService: { getBin: vi.fn() } }));

import { vi } from 'vitest';
import { databaseService, RegisteredUser } from './database.service';

describe('database.service — Facade', () => {
    it('exports databaseService object', () => {
        expect(databaseService).toBeDefined();
        expect(typeof databaseService).toBe('object');
    });

    it('merges picker service methods', () => {
        expect('getPicker' in databaseService).toBe(true);
    });

    it('merges attendance service methods', () => {
        expect('checkIn' in databaseService).toBe(true);
    });

    it('merges user service methods', () => {
        expect('getUser' in databaseService).toBe(true);
    });

    it('merges settings service methods', () => {
        expect('getSettings' in databaseService).toBe(true);
    });

    it('merges bin service methods', () => {
        expect('getBin' in databaseService).toBe(true);
    });

    it('RegisteredUser type has correct shape', () => {
        const user: RegisteredUser = {
            id: 'u1', full_name: 'Test User', role: 'manager',
        };
        expect(user.id).toBe('u1');
        expect(user.role).toBe('manager');
    });
});
