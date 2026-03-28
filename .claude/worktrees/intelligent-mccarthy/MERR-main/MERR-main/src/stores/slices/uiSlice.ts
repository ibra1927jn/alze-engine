/**
 * uiSlice — Global UI State (modals, drawers, selections)
 * 
 * Manages ephemeral UI state that needs to be accessible from any component
 * without prop-drilling. Currently handles the Picker Profile Drawer.
 */
import { StateCreator } from 'zustand';
import type { HarvestStoreState } from '../storeTypes';

export interface UISlice {
    /** Currently selected picker ID for the profile drawer (null = closed) */
    pickerProfileId: string | null;
    /** Open the picker profile drawer */
    openPickerProfile: (pickerId: string) => void;
    /** Close the picker profile drawer */
    closePickerProfile: () => void;
}

export const createUISlice: StateCreator<
    HarvestStoreState,
    [],
    [],
    UISlice
> = (set) => ({
    pickerProfileId: null,
    openPickerProfile: (pickerId) => set({ pickerProfileId: pickerId }),
    closePickerProfile: () => set({ pickerProfileId: null }),
});
