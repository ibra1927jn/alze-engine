/**
 * settingsSlice - Harvest Settings Management
 * 
 * Manages harvest_settings state and Supabase persistence.
 * Most isolated slice — only reads orchard.id and currentUser.id from global state.
 */
import { StateCreator } from 'zustand';
import { supabase } from '@/services/supabase';
import { auditService } from '@/services/audit.service';
import { logger } from '@/utils/logger';
import { HarvestSettings } from '@/types';
import type { HarvestStoreState, SettingsSlice } from '../storeTypes';

// --- Default State ---
export const defaultSettings: HarvestSettings = {
    min_wage_rate: 23.50,
    piece_rate: 6.50,
    min_buckets_per_hour: 3.6,
    target_tons: 100,
};

// --- Slice Creator ---
export const createSettingsSlice: StateCreator<
    HarvestStoreState,
    [],
    [],
    SettingsSlice
> = (set, get) => ({
    settings: defaultSettings,

    updateSettings: async (newSettings) => {
        const orchardId = get().orchard?.id;
        if (!orchardId) return;

        // Store previous state for audit + rollback
        const previousSettings = { ...get().settings };

        // 🔧 V13: Optimistic locking — check updated_at before writing
        const { data: current, error: fetchErr } = await supabase
            .from('harvest_settings')
            .select('updated_at')
            .eq('orchard_id', orchardId)
            .single();

        // Optimistic Update
        set((state) => ({ settings: { ...state.settings, ...newSettings } }));

        try {
            // 🔧 L2: count:'exact' required for OCC — without it, count is always undefined
            let query = supabase
                .from('harvest_settings')
                .update(newSettings, { count: 'exact' })
                .eq('orchard_id', orchardId);

            // If we got current timestamp, add OCC guard
            if (!fetchErr && current?.updated_at) {
                query = query.eq('updated_at', current.updated_at);
            }

            const { error, count } = await query;

            if (error) throw error;

            // OCC conflict: another tab/user changed settings since our read
            if (count === 0 && current?.updated_at) {
                logger.warn('⚠️ [Store] Settings OCC conflict — reloading from server');
                const { data: fresh } = await supabase
                    .from('harvest_settings')
                    .select('*')
                    .eq('orchard_id', orchardId)
                    .single();
                if (fresh) {
                    set({ settings: { ...previousSettings, ...fresh } });
                }
                return;
            }

            // 🔍 AUDIT LOG - Legal compliance tracking
            await auditService.logAudit(
                'settings.day_setup_modified',
                'Updated harvest settings',
                {
                    severity: 'info',
                    userId: get().currentUser?.id,
                    orchardId,
                    entityType: 'harvest_settings',
                    entityId: orchardId,
                    details: {
                        previous: previousSettings,
                        updated: newSettings,
                        changes: Object.keys(newSettings)
                    }
                }
            );
            logger.info('✅ [Store] Settings updated in Supabase');
        } catch (e) {
            logger.error('❌ [Store] Failed to update settings:', e);
            // Rollback
            set({ settings: previousSettings });
        }
    },
});
