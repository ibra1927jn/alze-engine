import { nowNZST } from '@/utils/nzst';
import { Bin } from '../types';
import { binRepository } from '@/repositories/bin.repository';

export const binService = {
    async getBins(orchardId: string): Promise<Bin[]> {
        const data = await binRepository.getByOrchard(orchardId);

        return (data || []).map(b => ({
            id: b.id,
            status: b.status,
            fillPercentage: 0, // Calculated in UI or derived
            type: ((b as Record<string, unknown>).variety as Bin['type']) || 'Standard',
            bin_code: b.bin_code
        }));
    },

    async updateBinStatus(binId: string, status: 'empty' | 'in-progress' | 'full' | 'collected') {
        await binRepository.updateStatus(binId, status, status === 'full' ? nowNZST() : null);
    }
};
