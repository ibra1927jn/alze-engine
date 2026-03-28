// =============================================
// SINGLE SUPABASE CLIENT
// =============================================
// IMPORTANT: This file ONLY exports the client.
// Do NOT create contexts or providers here.
// All other files should import from here.
// =============================================

import { createClient, SupabaseClient } from '@supabase/supabase-js';
import { getConfig } from './config.service';

// Get configuration (will throw in production if env vars missing)
const config = getConfig();
const SUPABASE_URL = config.SUPABASE_URL;
const SUPABASE_ANON_KEY = config.SUPABASE_ANON_KEY;

// Generate unique storageKey per tab to prevent BroadcastChannel sync
// BroadcastChannel uses storageKey as the channel name, so unique keys = no sync
// 🔧 Fix: sessionStorage is isolated per tab (localStorage was shared, defeating isolation)
const tabId = sessionStorage.getItem('harvestpro_tab_id') || `tab_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
sessionStorage.setItem('harvestpro_tab_id', tabId);

// Single client — do NOT create additional instances in other files
// Uses localStorage so sessions survive tab close/reopen (critical for field workers in low-signal zones)
export const supabase: SupabaseClient = createClient(SUPABASE_URL, SUPABASE_ANON_KEY, {
  auth: {
    persistSession: true,
    autoRefreshToken: true,
    storage: localStorage,
    storageKey: `sb-harvestpro-auth-${tabId}`, // Unique per tab to disable BroadcastChannel sync
  },
  realtime: {
    params: {
      eventsPerSecond: 10,
    },
  },
});

// Export URL and KEY in case they're needed elsewhere
export { SUPABASE_URL, SUPABASE_ANON_KEY };
