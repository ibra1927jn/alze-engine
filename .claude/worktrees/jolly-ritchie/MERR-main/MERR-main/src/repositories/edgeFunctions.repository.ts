/**
 * Edge Functions Repository — Supabase Edge Function invocation wrapper
 */
import { supabase } from '@/services/supabase';

export const edgeFunctionsRepository = {
    /** Invoke a Supabase Edge Function */
    async invoke<T = unknown>(functionName: string, body: Record<string, unknown>): Promise<{ data: T | null; error: { message: string } | null }> {
        const { data, error } = await supabase.functions.invoke(functionName, { body });
        return { data: data as T | null, error };
    },
};
