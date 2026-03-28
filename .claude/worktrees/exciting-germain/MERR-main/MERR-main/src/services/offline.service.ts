import { logger } from '@/utils/logger';
import { db, QueuedBucket } from './db';
import { toNZST } from '@/utils/nzst';

const DAYS_TO_KEEP_SYNCED = 7;

export interface QueueStats {
  pending: number;
  synced: number;
  errors: number;
  oldestPending: string | null;
}

export const offlineService = {
  /**
   * BUCKETS: Main Persistence
   */
  async queueBucket(bucket: Omit<QueuedBucket, 'synced'>) {
    try {
      // Save to Dexie with synced = 0 (Pending)
      await db.bucket_queue.put({ ...bucket, synced: 0 });
    } catch (error) {
      logger.error('❌ [Offline] Failed to queue bucket:', error);
    }
  },

  async getPendingBuckets(): Promise<QueuedBucket[]> {
    return await db.bucket_queue.where('synced').equals(0).toArray();
  },

  async markAsSynced(id: string) {
    try {
      await db.bucket_queue.update(id, { synced: 1 });
    } catch (error) {
      logger.error('❌ [Offline] Failed to mark as synced:', error);
    }
  },

  async getPendingCount(): Promise<number> {
    return await db.bucket_queue.where('synced').equals(0).count();
  },

  /**
   * CLEANUP: Remove old synced records to save space
   */
  async cleanupSynced() {
    const threshold = new Date();
    threshold.setDate(threshold.getDate() - DAYS_TO_KEEP_SYNCED);
    const isoThreshold = toNZST(threshold);

    await db.bucket_queue
      .filter(b => b.synced === 1 && b.timestamp < isoThreshold)
      .delete();
  },

  /**
   * Queue Statistics: pending, synced, errors, oldest pending item
   */
  // 🔧 V10: Use indexed Dexie queries instead of loading all records into memory
  async getQueueStats(): Promise<QueueStats> {
    const [pending, synced, errors] = await Promise.all([
      db.bucket_queue.where('synced').equals(0).count(),
      db.bucket_queue.where('synced').equals(1).count(),
      db.bucket_queue.where('synced').equals(-1).count(),
    ]);

    let oldestPending: string | null = null;
    if (pending > 0) {
      const oldest = await db.bucket_queue
        .where('synced').equals(0)
        .sortBy('timestamp');
      if (oldest.length > 0) {
        oldestPending = oldest[0].timestamp;
      }
    }

    return {
      pending,
      synced,
      errors,
      oldestPending,
    };
  },

  /**
   * LEGACY STUB: Conflict count (always 0, kept for compatibility)
   */
  getConflictCount: async () => 0,

  /**
   * RETRY: Reset all error items (synced = -1) back to pending (synced = 0)
   * Returns the number of items reset.
   */
  async retryFailedItems(): Promise<number> {
    const errors = await db.bucket_queue.where('synced').equals(-1).toArray();
    if (errors.length === 0) return 0;
    await Promise.all(
      errors.map(item => db.bucket_queue.update(item.id, { synced: 0 }))
    );
    return errors.length;
  },

  /**
   * ERROR ITEMS: List all failed items for debugging
   */
  async getErrorItems(): Promise<QueuedBucket[]> {
    return await db.bucket_queue.where('synced').equals(-1).toArray();
  },

  /**
   * CLEAR ERRORS: Remove error items older than specified hours (default: 24)
   */
  async clearErrorItems(olderThanHours = 24): Promise<number> {
    const threshold = new Date();
    threshold.setHours(threshold.getHours() - olderThanHours);
    const isoThreshold = toNZST(threshold);

    const toDelete = await db.bucket_queue
      .filter(b => b.synced === -1 && b.timestamp < isoThreshold)
      .toArray();

    await Promise.all(toDelete.map(item => db.bucket_queue.delete(item.id)));
    return toDelete.length;
  },
};