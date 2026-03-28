/**
 * _shared/security.ts — Reusable security helpers for Supabase Edge Functions
 *
 * Provides: CORS, auth verification, role-based access control, input validation,
 * and error sanitization.
 */
import { createClient, SupabaseClient } from 'https://esm.sh/@supabase/supabase-js@2'
import { z } from 'npm:zod@^3.22.4'

// ── CORS ─────────────────────────────────────────────

/** Production and preview origins allowed to call Edge Functions */
function isAllowedOrigin(origin: string | null): boolean {
    if (!origin) return false
    // Production domain
    if (origin === 'https://harvestpro.vercel.app') return true
    // Vercel preview deployments (any branch)
    if (origin.endsWith('.vercel.app')) return true
    // Local development
    if (origin === 'http://localhost:5173') return true
    if (origin === 'http://localhost:3000') return true
    return false
}

/**
 * Build CORS headers dynamically based on the request's Origin.
 * If the origin is allowed, reflect it back (not '*').
 * If not allowed, omit Access-Control-Allow-Origin to trigger browser CORS block.
 */
export function corsHeaders(origin: string | null): Record<string, string> {
    const headers: Record<string, string> = {
        'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
        'Access-Control-Allow-Methods': 'POST, OPTIONS',
        'Access-Control-Max-Age': '86400',
    }

    if (isAllowedOrigin(origin)) {
        headers['Access-Control-Allow-Origin'] = origin!
        headers['Vary'] = 'Origin'
    }

    return headers
}

/**
 * Handle OPTIONS preflight. Must be called at the top of every Edge Function.
 * Returns a Response if it's a preflight, or null if the request should continue.
 */
export function handlePreflight(req: Request): Response | null {
    if (req.method === 'OPTIONS') {
        return new Response('ok', {
            headers: corsHeaders(req.headers.get('Origin')),
        })
    }
    return null
}

// ── Auth & RBAC ──────────────────────────────────────

/** Allowed roles that can invoke management Edge Functions */
type AllowedRole = 'owner' | 'manager' | 'supervisor'

interface AuthResult {
    user: { id: string; email: string }
    supabase: SupabaseClient
}

/**
 * Verify the request has a valid JWT and the user has one of the allowed roles.
 * Creates a Supabase client scoped to the user's session.
 *
 * @throws Error with appropriate HTTP status context if auth fails
 */
export async function requireRole(
    req: Request,
    roles: AllowedRole[] = ['owner', 'manager']
): Promise<AuthResult> {
    const authHeader = req.headers.get('Authorization')
    if (!authHeader) {
        throw new AuthError('Missing authorization header', 401)
    }

    // Create Supabase client with the user's JWT
    const supabase = createClient(
        Deno.env.get('SUPABASE_URL') ?? '',
        Deno.env.get('SUPABASE_ANON_KEY') ?? '',
        { global: { headers: { Authorization: authHeader } } }
    )

    // Verify the JWT is valid and get the user
    const { data: { user }, error: userError } = await supabase.auth.getUser()
    if (userError || !user) {
        throw new AuthError('Invalid or expired token', 401)
    }

    // Check role from user metadata
    const userRole = user.user_metadata?.role as string | undefined
    if (!userRole || !roles.includes(userRole as AllowedRole)) {
        throw new AuthError(
            `Insufficient permissions. Required: ${roles.join('|')}. Got: ${userRole || 'none'}`,
            403
        )
    }

    return {
        user: { id: user.id, email: user.email ?? '' },
        supabase,
    }
}

// ── Custom Error ─────────────────────────────────────

export class AuthError extends Error {
    status: number
    constructor(message: string, status: number) {
        super(message)
        this.name = 'AuthError'
        this.status = status
    }
}

// ── Error Sanitization ───────────────────────────────

/**
 * Build a safe error response. Never leaks stack traces.
 * Logs the full error server-side for debugging.
 */
export function errorResponse(
    error: unknown,
    origin: string | null,
    context: string
): Response {
    const isAuthError = error instanceof AuthError

    // Log full error server-side (visible in Supabase dashboard logs)
    console.error(`[${context}] Error:`, error)

    const status = isAuthError ? error.status : 400
    const message = error instanceof Error ? error.message : 'Unknown error'

    return new Response(
        JSON.stringify({
            error: isAuthError ? message : 'Request failed. Check parameters and try again.',
            ...(isAuthError ? {} : { hint: message }),
        }),
        {
            status,
            headers: {
                ...corsHeaders(origin),
                'Content-Type': 'application/json',
            },
        }
    )
}

// ── Input Validation Schemas ─────────────────────────

export const PayrollInputSchema = z.object({
    orchard_id: z.string().uuid('orchard_id must be a valid UUID'),
    start_date: z.string().regex(/^\d{4}-\d{2}-\d{2}$/, 'start_date must be YYYY-MM-DD'),
    end_date: z.string().regex(/^\d{4}-\d{2}-\d{2}$/, 'end_date must be YYYY-MM-DD'),
})

export const AnomalyInputSchema = z.object({
    orchard_id: z.string().uuid('orchard_id must be a valid UUID'),
})

export type PayrollInput = z.infer<typeof PayrollInputSchema>
export type AnomalyInput = z.infer<typeof AnomalyInputSchema>
