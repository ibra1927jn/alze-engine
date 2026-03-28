/**
 * RPC Repository — Supabase RPC (Remote Procedure Call) wrapper
 * 
 * Centralizes all Supabase RPC calls so services don't need
 * direct supabase imports for function calls.
 */
import { supabase } from '@/services/supabase';

export const rpcRepository = {
    /** Call a Supabase RPC function with parameters */
    async call<T = unknown>(functionName: string, params: Record<string, unknown>): Promise<{ data: T | null; error: { code?: string; message: string } | null }> {
        const { data, error } = await supabase.rpc(functionName, params);
        return { data: data as T | null, error };
    },
};
