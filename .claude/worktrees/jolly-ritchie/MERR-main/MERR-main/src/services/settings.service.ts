import { logger } from '@/utils/logger';
import { settingsRepository } from '@/repositories/settings.repository';
import { HarvestSettings } from '../types';

export const settingsService = {
    // --- SETTINGS ---
    async getHarvestSettings(orchardId: string): Promise<HarvestSettings | null> {
        const data = await settingsRepository.getByOrchardId(orchardId);
        if (!data) return null;

        return {
            min_wage_rate: data.min_wage_rate,
            piece_rate: data.piece_rate,
            min_buckets_per_hour: data.min_buckets_per_hour,
            target_tons: data.target_tons,
            variety: data.variety,
        };
    },

    async updateHarvestSettings(
        orchardId: string,
        updates: Partial<HarvestSettings>
    ): Promise<boolean> {
        try {
            await settingsRepository.upsert(orchardId, updates);
            return true;
        } catch (error) {
            logger.error('[SettingsService] Failed to update settings:', (error as Error).message);
            return false;
        }
    },
};
