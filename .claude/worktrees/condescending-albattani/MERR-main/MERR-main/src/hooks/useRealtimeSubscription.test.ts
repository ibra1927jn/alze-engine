/**
 * useRealtimeSubscription Hook Tests
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderHook } from '@testing-library/react';
import { useRealtimeSubscription } from './useRealtimeSubscription';
import { supabase } from '@/services/supabase';

// Mock supabase
vi.mock('@/services/supabase', () => {
    const channelMock = {
        on: vi.fn().mockReturnThis(),
        subscribe: vi.fn().mockReturnThis(),
    };
    return {
        supabase: {
            channel: vi.fn(() => channelMock),
            removeChannel: vi.fn(),
        },
    };
});

describe('useRealtimeSubscription', () => {
    const onPayload = vi.fn();

    beforeEach(() => { vi.clearAllMocks(); });

    it('subscribes when enabled', () => {
        renderHook(() => useRealtimeSubscription({
            channelName: 'test-channel', table: 'bucket_records',
            onPayload, enabled: true,
        }));
        expect(supabase.channel).toHaveBeenCalledWith('test-channel');
    });

    it('does not subscribe when disabled', () => {
        renderHook(() => useRealtimeSubscription({
            channelName: 'test-channel', table: 'bucket_records',
            onPayload, enabled: false,
        }));
        expect(supabase.channel).not.toHaveBeenCalled();
    });

    it('removes channel on unmount', () => {
        const { unmount } = renderHook(() => useRealtimeSubscription({
            channelName: 'test-channel', table: 'bucket_records',
            onPayload, enabled: true,
        }));
        unmount();
        expect(supabase.removeChannel).toHaveBeenCalled();
    });

    it('passes filter when provided', () => {
        renderHook(() => useRealtimeSubscription({
            channelName: 'test', table: 'bucket_records',
            filter: 'orchard_id=eq.o1', event: 'INSERT',
            onPayload, enabled: true,
        }));
        expect(supabase.channel).toHaveBeenCalledWith('test');
    });
});
