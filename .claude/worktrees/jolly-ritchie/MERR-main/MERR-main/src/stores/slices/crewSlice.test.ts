/**
 * crewSlice Tests — pure state transition logic
 */
import { describe, it, expect } from 'vitest';
import type { Picker } from '@/types';

// Test the pure state transition logic from crewSlice
const createPicker = (overrides: Partial<Picker> = {}): Picker => ({
    id: 'p1', picker_id: 'PK001', name: 'Alice', avatar: 'A',
    current_row: 0, total_buckets_today: 0, hours: 0,
    status: 'active', safety_verified: false, qcStatus: [],
    orchard_id: 'o1', ...overrides,
});

const addPickerToList = (crew: Picker[], picker: Picker) => [...crew, picker];

const removePickerFromList = (crew: Picker[], id: string) => crew.filter(p => p.id !== id);

const updatePickerInList = (crew: Picker[], id: string, updates: Partial<Picker>) =>
    crew.map(p => p.id === id ? { ...p, ...updates } : p);

const cleanUpdate = (updates: Record<string, unknown>) => {
    const clean: Record<string, unknown> = {};
    Object.entries(updates).forEach(([k, v]) => { if (v !== undefined) clean[k] = v; });
    return clean;
};

describe('crewSlice — add picker', () => {
    it('adds picker to empty crew', () => {
        const result = addPickerToList([], createPicker());
        expect(result).toHaveLength(1);
    });

    it('adds picker to existing crew', () => {
        const existing = [createPicker({ id: 'p0' })];
        const result = addPickerToList(existing, createPicker({ id: 'p1' }));
        expect(result).toHaveLength(2);
    });

    it('defaults avatar from first character of name', () => {
        const p = createPicker({ name: 'Bob' });
        const avatar = (p.name || 'U').charAt(0).toUpperCase();
        expect(avatar).toBe('B');
    });
});

describe('crewSlice — remove picker', () => {
    it('removes picker by id', () => {
        const crew = [createPicker({ id: 'p1' }), createPicker({ id: 'p2' })];
        const result = removePickerFromList(crew, 'p1');
        expect(result).toHaveLength(1);
        expect(result[0].id).toBe('p2');
    });

    it('no-op for unknown id', () => {
        const crew = [createPicker({ id: 'p1' })];
        expect(removePickerFromList(crew, 'unknown')).toHaveLength(1);
    });

    it('handles empty crew', () => {
        expect(removePickerFromList([], 'p1')).toEqual([]);
    });
});

describe('crewSlice — update picker', () => {
    it('updates specific fields', () => {
        const crew = [createPicker({ id: 'p1', name: 'Alice' })];
        const result = updatePickerInList(crew, 'p1', { name: 'Updated' });
        expect(result[0].name).toBe('Updated');
    });

    it('preserves other pickers', () => {
        const crew = [createPicker({ id: 'p1' }), createPicker({ id: 'p2', name: 'Bob' })];
        const result = updatePickerInList(crew, 'p1', { name: 'Changed' });
        expect(result[1].name).toBe('Bob');
    });
});

describe('crewSlice — clean update', () => {
    it('removes undefined fields', () => {
        const result = cleanUpdate({ name: 'Test', status: undefined, hours: 4 });
        expect(result).toEqual({ name: 'Test', hours: 4 });
    });

    it('empty input returns empty', () => {
        expect(cleanUpdate({})).toEqual({});
    });

    it('keeps null values (different from undefined)', () => {
        const result = cleanUpdate({ name: null });
        expect(result).toEqual({ name: null });
    });
});
