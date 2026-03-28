/**
 * useRealtimeSubscription.ts — Reusable Supabase Realtime Hook
 *
 * Generic hook that subscribes to a Supabase realtime channel
 * and calls a callback when changes arrive. Handles cleanup automatically.
 *
 * Usage:
 *   useRealtimeSubscription({
 *     channelName: `qc-${orchardId}`,
 *     table: 'qc_inspections',
 *     filter: `orchard_id=eq.${orchardId}`,
 *     event: 'INSERT',
 *     onPayload: (payload) => { ... },
 *     enabled: !!orchardId,
 *   });
 */
import { useEffect, useRef } from 'react';
import { supabase } from '@/services/supabase';
import { logger } from '@/utils/logger';
import type { RealtimeChannel } from '@supabase/supabase-js';

interface UseRealtimeOptions {
    /** Unique channel name */
    channelName: string;
    /** Supabase table to listen on */
    table: string;
    /** Optional row-level filter: `column=eq.value` */
    filter?: string;
    /** Postgres event type */
    event?: 'INSERT' | 'UPDATE' | 'DELETE' | '*';
    /** Callback on incoming payload */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    onPayload: (payload: any) => void;
    /** Enable/disable subscription (e.g. when orchardId is available) */
    enabled?: boolean;
}

export function useRealtimeSubscription({
    channelName,
    table,
    filter,
    event = '*',
    onPayload,
    enabled = true,
}: UseRealtimeOptions) {
    const channelRef = useRef<RealtimeChannel | null>(null);

    useEffect(() => {
        if (!enabled) return;

        logger.info(`[Realtime] Subscribing to ${table} on channel ${channelName}`);

        // Build channel with postgres_changes listener
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const channelConfig: any = {
            event,
            schema: 'public',
            table,
        };
        if (filter) channelConfig.filter = filter;

        const channel = supabase
            .channel(channelName)
            .on('postgres_changes', channelConfig, (payload) => {
                logger.info(`[Realtime] ${table} ${payload.eventType}:`, payload.new);
                onPayload(payload);
            })
            .subscribe((status) => {
                logger.info(`[Realtime] ${channelName} status: ${status}`);
            });

        channelRef.current = channel;

        return () => {
            logger.info(`[Realtime] Unsubscribing from ${channelName}`);
            if (channelRef.current) {
                supabase.removeChannel(channelRef.current);
                channelRef.current = null;
            }
        };
        // We intentionally only re-subscribe when key dependencies change
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [channelName, table, filter, event, enabled]);
}
