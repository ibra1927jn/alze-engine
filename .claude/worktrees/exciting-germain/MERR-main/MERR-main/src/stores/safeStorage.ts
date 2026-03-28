/**
 * safeStorage — localStorage wrapper with QuotaExceededError handling
 * Falls back to Dexie/IndexedDB when localStorage is full.
 */
import { logger } from '@/utils/logger';

export const safeStorage = {
    getItem: (name: string) => localStorage.getItem(name),
    setItem: (name: string, value: string) => {
        try {
            localStorage.setItem(name, value);
        } catch (e) {
            if (e instanceof DOMException && e.name === 'QuotaExceededError') {
                logger.error('[Storage] QuotaExceededError - clearing old data');
                const keysToKeep = ['harvest-pro-storage', 'harvest-pro-recovery'];
                const keysToRemove: string[] = [];
                for (let i = 0; i < localStorage.length; i++) {
                    const key = localStorage.key(i);
                    if (key && !keysToKeep.includes(key)) {
                        keysToRemove.push(key);
                    }
                }
                keysToRemove.forEach(k => localStorage.removeItem(k));
                try {
                    localStorage.setItem(name, value);
                    logger.info('[Storage] Retry succeeded after cleanup');
                } catch (e) {
                    logger.error('[Storage] Still over quota after cleanup', e);
                    try {
                        const parsed = JSON.parse(value);
                        const criticalData = {
                            state: {
                                buckets: parsed?.state?.buckets?.filter((b: { synced: boolean }) => !b.synced) || [],
                                currentUser: parsed?.state?.currentUser,
                            }
                        };
                        import('@/services/db').then(({ db }) => {
                            db.table('recovery').put({ id: 'quota-crash', data: criticalData, timestamp: Date.now() })
                                .catch((dbErr: unknown) => logger.error('[Storage] Dexie recovery also failed:', dbErr));
                        });
                    } catch (e) {
                        logger.error('[Storage] Recovery key also failed — data may be lost', e);
                    }
                }
            }
        }
    },
    removeItem: (name: string) => localStorage.removeItem(name),
};
