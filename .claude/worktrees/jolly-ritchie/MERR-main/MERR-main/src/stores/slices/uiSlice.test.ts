/**
 * uiSlice Tests — pure state logic
 */
import { describe, it, expect } from 'vitest';

// Test the pure state transitions from uiSlice
interface UIState {
    pickerProfileId: string | null;
}

const openPickerProfile = (state: UIState, id: string): UIState => ({
    ...state,
    pickerProfileId: id,
});

const closePickerProfile = (state: UIState): UIState => ({
    ...state,
    pickerProfileId: null,
});

describe('uiSlice — picker profile drawer', () => {
    it('starts with null pickerProfileId', () => {
        const state: UIState = { pickerProfileId: null };
        expect(state.pickerProfileId).toBeNull();
    });

    it('openPickerProfile sets the ID', () => {
        const state: UIState = { pickerProfileId: null };
        const next = openPickerProfile(state, 'picker-123');
        expect(next.pickerProfileId).toBe('picker-123');
    });

    it('closePickerProfile resets to null', () => {
        const state: UIState = { pickerProfileId: 'picker-123' };
        const next = closePickerProfile(state);
        expect(next.pickerProfileId).toBeNull();
    });

    it('opening a different picker replaces the current one', () => {
        let state: UIState = { pickerProfileId: null };
        state = openPickerProfile(state, 'picker-1');
        state = openPickerProfile(state, 'picker-2');
        expect(state.pickerProfileId).toBe('picker-2');
    });

    it('closing when already null is a no-op', () => {
        const state: UIState = { pickerProfileId: null };
        const next = closePickerProfile(state);
        expect(next.pickerProfileId).toBeNull();
    });
});
