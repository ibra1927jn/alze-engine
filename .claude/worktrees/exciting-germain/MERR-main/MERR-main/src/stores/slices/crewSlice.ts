/**
 * crewSlice - Picker/Crew Management
 * 
 * Manages crew state (pickers) and CRUD operations via Supabase.
 * Reads orchard.id and currentUser.id from global state via get().
 */
import { StateCreator } from 'zustand';
import { supabase } from '@/services/supabase';
import { syncService } from '@/services/sync.service';
import type { PickerPayload } from '@/services/sync-processors/types';
import { auditService } from '@/services/audit.service';
import { logger } from '@/utils/logger';
import { safeUUID } from '@/utils/uuid';
import { Picker } from '@/types';
import type { HarvestStoreState, CrewSlice } from '../storeTypes';

// --- Slice Creator ---
export const createCrewSlice: StateCreator<
    HarvestStoreState,
    [],
    [],
    CrewSlice
> = (set, get) => ({
    crew: [],
    presentCount: 0,

    addPicker: async (picker) => {
        const orchardId = get().orchard?.id;
        if (!orchardId) return;

        // 🔧 V20: Generate UUID on frontend (not Supabase) to prevent zombie picker race
        const stableId = safeUUID();
        const newPicker: Picker = {
            id: stableId,
            picker_id: picker.picker_id || '',
            name: picker.name || 'Unknown',
            avatar: picker.avatar || (picker.name || 'U').charAt(0).toUpperCase(),
            current_row: picker.current_row || 0,
            total_buckets_today: picker.total_buckets_today || 0,
            hours: picker.hours || 0,
            role: picker.role || 'picker',
            orchard_id: orchardId,
            status: picker.status || 'active',
            safety_verified: picker.safety_verified || false,
            qcStatus: picker.qcStatus || [],
            team_leader_id: picker.team_leader_id ?? undefined,
        };
        set(state => ({ crew: [...state.crew, newPicker] }));

        // 🔧 L26: Queue via syncService when offline — prevents picker disappearing
        // when there's no internet in the orchard
        if (!navigator.onLine) {
            await syncService.addToQueue('PICKER', {
                ...newPicker,
            } as PickerPayload);
            logger.info('📡 [Store] Picker queued for offline sync');
            return;
        }

        try {
            const { data, error } = await supabase
                .from('pickers')
                .insert({
                    ...newPicker,
                })
                .select()
                .single();

            if (error) throw error;

            // Update with any server-side defaults (e.g., created_at)
            set(state => ({
                crew: state.crew.map(p => p.id === stableId ? { ...newPicker, ...data } : p)
            }));
            logger.info('✅ [Store] Picker added to Supabase');
        } catch (e) {
            // 🔧 L26: On network error, queue instead of destroying
            if (e instanceof Error && (e.message.includes('fetch') || e.message.includes('network'))) {
                await syncService.addToQueue('PICKER', {
                    ...newPicker,
                } as PickerPayload);
                logger.warn('📡 [Store] Network error — picker queued for sync');
            } else {
                logger.error('❌ [Store] Failed to add picker:', e);
                set(state => ({ crew: state.crew.filter(p => p.id !== stableId) }));
            }
        }
    },

    removePicker: async (id) => {
        const orchardId = get().orchard?.id;
        if (!orchardId) return;

        // 📋 Soft-delete: mark as archived instead of hard delete
        const pickerToRemove = get().crew.find(p => p.id === id);
        set(state => ({ crew: state.crew.filter(p => p.id !== id) }));

        try {
            const { error } = await supabase
                .from('pickers')
                .update({ status: 'archived' })
                .eq('id', id);

            if (error) throw error;

            // 🔍 Audit log the soft-delete
            await auditService.logAudit(
                'picker.deleted',
                `Soft-deleted picker: ${pickerToRemove?.name}`,
                {
                    severity: 'warning',
                    userId: get().currentUser?.id,
                    orchardId,
                    entityType: 'picker',
                    entityId: id,
                    details: { picker: pickerToRemove }
                }
            );
            logger.info(`🗑️ [Store] Picker ${id} archived in Supabase`);
        } catch (e) {
            logger.error('❌ [Store] Failed to archive picker:', e);
            // Rollback
            if (pickerToRemove) {
                set(state => ({ crew: [...state.crew, pickerToRemove] }));
            }
        }
    },

    updatePicker: async (id, updates) => {
        const previous = get().crew.find(p => p.id === id);
        // Optimistic
        set(state => ({
            crew: state.crew.map(p => p.id === id ? { ...p, ...updates } : p)
        }));

        try {
            // Build clean update (no undefined fields)
            const cleanUpdate: Record<string, unknown> = {};
            Object.entries(updates).forEach(([key, value]) => {
                if (value !== undefined) cleanUpdate[key] = value;
            });

            const { error } = await supabase
                .from('pickers')
                .update(cleanUpdate)
                .eq('id', id);

            if (error) throw error;
            logger.info(`✅ [Store] Picker ${id} updated in Supabase`);
        } catch (e) {
            logger.error('❌ [Store] Failed to update picker:', e);
            // Rollback
            if (previous) {
                set(state => ({
                    crew: state.crew.map(p => p.id === id ? previous : p)
                }));
            }
        }
    },

    unassignUser: async (id) => {
        // 🔧 V8: Save picker for rollback before removal
        const pickerToRestore = get().crew.find(p => p.id === id);
        set(state => ({
            crew: state.crew.filter(p => p.id !== id)
        }));
        try {
            const { error } = await supabase
                .from('pickers')
                .update({ orchard_id: null })
                .eq('id', id);
            if (error) throw error;
            logger.info(`🔓 [Store] User ${id} unassigned from orchard`);
        } catch (e) {
            logger.error('❌ [Store] Failed to unassign user:', e);
            // Rollback: restore picker to crew
            if (pickerToRestore) {
                set(state => ({ crew: [...state.crew, pickerToRestore] }));
            }
        }
    },
});
