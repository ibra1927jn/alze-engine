import { useEffect, useRef, useCallback } from 'react';
import { useHarvestStore } from '../../stores/useHarvestStore';
import { bucketEventsRepository } from '@/repositories/bucketEvents.repository';
import { offlineService } from '../../services/offline.service';
import { logger } from '@/utils/logger';

const BASE_DELAY = 5_000;    // 5 seconds
const MAX_DELAY = 300_000;   // 5 minutes cap

/**
 * Invisible sync bridge that batch-uploads pending buckets to Supabase.
 * 
 * FIX C3: Uses getState() to avoid stale closure on buckets array.
 * 
 * IDEMPOTENCY: If `bucket_events.id` has a UNIQUE constraint (migration),
 * duplicate inserts from retry attempts receive a 409/23505 error.
 * We treat these as success — the data already exists in the DB.
 */
export const HarvestSyncBridge = () => {
    const retryDelay = useRef(BASE_DELAY);
    const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
    const syncPendingBucketsRef = useRef<(() => Promise<void>) | null>(null);

    // Stable scheduleNext using ref to avoid circular dependency
    const scheduleNext = useCallback(() => {
        if (timerRef.current) clearTimeout(timerRef.current);
        timerRef.current = setTimeout(() => {
            syncPendingBucketsRef.current?.();
        }, retryDelay.current);
    }, []); // Empty deps — uses ref, so stable forever

    // FIX C3: Stable callback — reads from store directly, no stale closure
    const syncPendingBuckets = useCallback(async () => {
        const { buckets, markAsSynced } = useHarvestStore.getState();
        const pending = buckets.filter(b => !b.synced);

        if (pending.length === 0) {
            retryDelay.current = BASE_DELAY;
            scheduleNext();
            return;
        }

        try {
            const rows = pending.map(b => ({
                id: b.id,
                picker_id: b.picker_id,
                quality_grade: b.quality_grade,
                orchard_id: b.orchard_id,
                recorded_at: b.timestamp,
            }));

            const { error } = await bucketEventsRepository.insertBatch(rows);

            if (!error) {
                pending.forEach(b => markAsSynced(b.id));
                retryDelay.current = BASE_DELAY;

                offlineService.cleanupSynced().catch(e =>
                    logger.error('[Bridge] Cleanup failed:', e)
                );
            } else if (error.code === '23505') {
                logger.info('[Bridge] ⚡ Duplicate detected (23505), resolving individually');


                for (const b of pending) {
                    const { error: singleError } = await bucketEventsRepository.insertSingle({
                        id: b.id,
                        picker_id: b.picker_id,
                        quality_grade: b.quality_grade,
                        orchard_id: b.orchard_id,
                        recorded_at: b.timestamp,
                    });

                    if (!singleError || singleError.code === '23505') {
                        markAsSynced(b.id);
                    }
                }
                retryDelay.current = BASE_DELAY;
            } else {

                logger.error('[Bridge] Batch insert error:', error.message);
                retryDelay.current = Math.min(retryDelay.current * 2, MAX_DELAY);
            }
        } catch (e) {

            logger.error('[Bridge] Network error:', e);
            retryDelay.current = Math.min(retryDelay.current * 2, MAX_DELAY);
        }

        scheduleNext();
    }, [scheduleNext]); // scheduleNext is stable (empty deps), safe to include

    // Update ref whenever syncPendingBuckets changes
    useEffect(() => {
        syncPendingBucketsRef.current = syncPendingBuckets;
    }, [syncPendingBuckets]);

    useEffect(() => {
        syncPendingBuckets();
        return () => {
            if (timerRef.current) clearTimeout(timerRef.current);
        };
    }, [syncPendingBuckets]);

    return null;
};