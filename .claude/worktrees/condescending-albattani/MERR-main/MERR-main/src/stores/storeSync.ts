/**
 * storeSync — Sync logic extracted from useHarvestStore
 *   hydrateFromRecovery    — Recover crash-saved buckets from localStorage
 *   hydrateFromDexie       — Recover pending buckets from IndexedDB
 *   fetchOrchardData       — Delta/full sync with Supabase
 *   setupRealtimeSubscriptions — Supabase real-time channels
 */
import { supabase } from '@/services/supabase';
import { offlineService } from '@/services/offline.service';
import { logger } from '@/utils/logger';
import { todayNZST, toNZST } from '@/utils/nzst';
import { BucketRecord } from '@/types';
import type { HarvestStoreState, ScannedBucket } from './storeTypes';
import { storeSyncRepository } from '@/repositories/storeSync.repository';

export type StoreSetter = (
    partial: Partial<HarvestStoreState> | ((state: HarvestStoreState) => Partial<HarvestStoreState>)
) => void;
export type StoreGetter = () => HarvestStoreState;

/* ── Hydration ────────────────────────────── */

export function hydrateFromRecovery(set: StoreSetter): void {
    try {
        const recoveryData = localStorage.getItem('harvest-pro-recovery');
        if (recoveryData) {
            const parsed = JSON.parse(recoveryData);
            const recoveredBuckets = parsed?.state?.buckets || [];
            if (recoveredBuckets.length > 0) {
                set((state) => {
                    const existingIds = new Set(state.buckets.map(b => b.id));
                    const uniqueRecovered = recoveredBuckets.filter(
                        (b: ScannedBucket) => !existingIds.has(b.id)
                    );
                    if (uniqueRecovered.length > 0) {
                        logger.info(`[Store] Recovered ${uniqueRecovered.length} buckets from crash backup`);
                        return { buckets: [...uniqueRecovered, ...state.buckets] };
                    }
                    return state;
                });
            }
            localStorage.removeItem('harvest-pro-recovery');
            logger.info('[Store] Recovery data consumed and cleared');
        }
    } catch (e) {
        logger.error('[Store] Failed to hydrate from recovery:', e);
        localStorage.removeItem('harvest-pro-recovery');
    }
}

export async function hydrateFromDexie(set: StoreSetter): Promise<void> {
    try {
        const pendingBuckets = await offlineService.getPendingBuckets();
        if (pendingBuckets.length > 0) {
            set((state) => {
                const existingIds = new Set(state.buckets.map(b => b.id));
                const uniquePending = pendingBuckets
                    .filter(b => !existingIds.has(String(b.id)))
                    .map(pb => ({ ...pb, id: String(pb.id), synced: false }));
                if (uniquePending.length > 0) {
                    logger.info(`[Store] Hydrated ${uniquePending.length} pending buckets from Dexie`);
                    return { buckets: [...uniquePending, ...state.buckets] };
                }
                return state;
            });
        }
    } catch (e) {
        logger.error('[Store] Failed to hydrate from Dexie:', e);
    }
}

/* ── Fetch Orchard Data (Delta/Full Sync) ── */

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export async function fetchOrchardData(get: StoreGetter, set: StoreSetter): Promise<any> {
    const state = get();
    const lastSync = state.lastSyncAt;
    const now = new Date().toISOString();

    const isStale = !lastSync || (Date.now() - new Date(lastSync).getTime() > 24 * 60 * 60 * 1000);
    const useDelta = !isStale;

    // Always fetch orchard + settings (tiny)
    const activeOrchard = await storeSyncRepository.getFirstOrchard();

    const settings = activeOrchard ? await storeSyncRepository.getSettings(activeOrchard.id) : null;

    set({ orchard: activeOrchard, settings: settings || state.settings });

    // Pickers + Attendance
    const today = todayNZST();
    const startOfDayNZ = toNZST((() => { const d = new Date(); d.setHours(0, 0, 0, 0); return d; })());

    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const mapPickerWithAttendance = (p: any) => ({
        ...p,
        checked_in_today: !!p.daily_attendance?.[0]?.check_in_time,
        check_in_time: p.daily_attendance?.[0]?.check_in_time || null,
        daily_attendance: undefined,
    });

    try {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        let pickersQuery: any = storeSyncRepository.getPickersQuery(activeOrchard?.id, today);

        let bucketsQuery = storeSyncRepository.getBucketRecordsQuery(activeOrchard?.id, startOfDayNZ);

        if (useDelta) {
            const safeSyncDate = new Date(new Date(lastSync!).getTime() - 120_000).toISOString();
            pickersQuery = pickersQuery.gte('updated_at', safeSyncDate);
            bucketsQuery = bucketsQuery.gte('updated_at', safeSyncDate);
            logger.info(`[Sync] Delta mode: fetching changes since ${safeSyncDate}`);
        } else {
            pickersQuery = pickersQuery.is('deleted_at', null);
            bucketsQuery = bucketsQuery.is('deleted_at', null);
            logger.info('[Sync] Full mode: downloading all active records');
        }

        const [pickersRes, bucketsRes] = await Promise.all([pickersQuery, bucketsQuery]);
        if (pickersRes.error) throw pickersRes.error;
        if (bucketsRes.error) throw bucketsRes.error;

        if (useDelta) {
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            const crewMap = new Map(state.crew.map((p: any) => [p.id, p]));
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            pickersRes.data.forEach((p: any) => {
                if (p.deleted_at) crewMap.delete(p.id);
                else crewMap.set(p.id, mapPickerWithAttendance(p));
            });
            const bucketsMap = new Map(state.bucketRecords.map((b: BucketRecord) => [b.id, b]));
            bucketsRes.data.forEach((b: BucketRecord) => {
                if (b.deleted_at) bucketsMap.delete(b.id);
                else bucketsMap.set(b.id, b);
            });
            set({ crew: Array.from(crewMap.values()), bucketRecords: Array.from(bucketsMap.values()), lastSyncAt: now });
            logger.info(`[Sync] Delta applied: ${pickersRes.data.length} pickers, ${bucketsRes.data.length} buckets`);
        } else {
            const crewWithAttendance = pickersRes.data?.map(mapPickerWithAttendance) || [];
            set({ crew: crewWithAttendance, bucketRecords: bucketsRes.data || [], lastSyncAt: now });
            logger.info(`[Sync] Full load: ${crewWithAttendance.length} pickers, ${(bucketsRes.data || []).length} buckets`);
        }
    } catch (syncError) {
        logger.warn('[Sync] Delta/full sync failed — falling back to simple fetch:', syncError);
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const [pickersRes, bucketsRes] = await Promise.all([
            storeSyncRepository.getPickersQuery(activeOrchard?.id, today),
            storeSyncRepository.getBucketRecordsQuery(activeOrchard?.id, startOfDayNZ),
        ]);
        const crewWithAttendance = pickersRes.data?.map(mapPickerWithAttendance) || [];
        set({ crew: crewWithAttendance, bucketRecords: bucketsRes.data || [], lastSyncAt: null });
        logger.info(`[Sync] Fallback load: ${crewWithAttendance.length} pickers, ${(bucketsRes.data || []).length} buckets`);
    }

    // Fetch blocks
    if (activeOrchard?.id) await get().fetchBlocks(activeOrchard.id);

    // Rebuild rowAssignments if empty
    const existingAssignments = get().rowAssignments;
    if (existingAssignments.length === 0) {
        const crewList = get().crew;
        const rowMap = new Map<string, { row: number; pickers: string[] }>();
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        for (const p of crewList as any[]) {
            if (p.current_row > 0) {
                const groupKey = p.team_leader_id || p.id;
                const mapKey = `${groupKey}-${p.current_row}`;
                if (!rowMap.has(mapKey)) rowMap.set(mapKey, { row: p.current_row, pickers: [] });
                rowMap.get(mapKey)!.pickers.push(p.id);
            }
        }
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        for (const p of crewList as any[]) {
            if (p.current_row > 0 && p.role === 'team_leader') {
                const mapKey = `${p.id}-${p.current_row}`;
                if (!rowMap.has(mapKey)) rowMap.set(mapKey, { row: p.current_row, pickers: [] });
                if (!rowMap.get(mapKey)!.pickers.includes(p.id)) rowMap.get(mapKey)!.pickers.push(p.id);
            }
        }
        const rebuiltAssignments = Array.from(rowMap.values()).map(entry => ({
            id: `rebuilt-${entry.row}-${entry.pickers[0]}`,
            row_number: entry.row,
            side: 'north' as const,
            assigned_pickers: entry.pickers,
            completion_percentage: 0,
        }));
        if (rebuiltAssignments.length > 0) {
            set({ rowAssignments: rebuiltAssignments });
            logger.info(`[Store] Rebuilt ${rebuiltAssignments.length} row assignments from crew data`);
        }
    } else {
        logger.info(`[Store] Using ${existingAssignments.length} persisted row assignments`);
    }

    return activeOrchard;
}

/* ── Realtime Subscriptions ─────────────── */

export function setupRealtimeSubscriptions(orchardId: string, get: StoreGetter, set: StoreSetter): void {
    logger.info('[Store] Setting up real-time subscriptions (single channel)...');
    supabase.removeAllChannels();

    supabase.channel(`harvest-live-${orchardId}`)
        .on('postgres_changes', { event: 'INSERT', schema: 'public', table: 'bucket_records', filter: `orchard_id=eq.${orchardId}` }, (payload) => {
            if (document.hidden) return;
            logger.info('[Store] Real-time bucket record received');
            set((state) => {
                const newRecord = payload.new as BucketRecord;
                if (state.bucketRecords.some(b => b.id === newRecord.id)) return state;
                return { bucketRecords: [newRecord, ...state.bucketRecords] };
            });
            get().recalculateIntelligence();
        })
        .on('postgres_changes', { event: '*', schema: 'public', table: 'daily_attendance', filter: `orchard_id=eq.${orchardId}` }, (payload) => {
            if (document.hidden) return;
            const todayStr = todayNZST();
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            const attendanceRecord = payload.new as any;
            if (attendanceRecord && attendanceRecord.date === todayStr) {
                set((state) => ({
                    crew: state.crew.map(p =>
                        p.id === attendanceRecord.picker_id
                            ? { ...p, checked_in_today: !!attendanceRecord.check_in_time, check_in_time: attendanceRecord.check_in_time }
                            : p
                    ),
                }));
            }
        })
        .on('postgres_changes', { event: 'INSERT', schema: 'public', table: 'qc_inspections', filter: `orchard_id=eq.${orchardId}` }, (payload) => {
            if (document.hidden) return;
            set(state => ({ recentQcInspections: [payload.new as Record<string, unknown>, ...state.recentQcInspections].slice(0, 10) }));
        })
        .on('postgres_changes', { event: '*', schema: 'public', table: 'timesheets', filter: `orchard_id=eq.${orchardId}` }, (payload) => {
            if (document.hidden) return;
            set(state => ({ recentTimesheetUpdates: [payload.new as Record<string, unknown>, ...state.recentTimesheetUpdates].slice(0, 10) }));
        })
        .subscribe((status) => logger.info(`[Store] Realtime subscription: ${status}`));
}
