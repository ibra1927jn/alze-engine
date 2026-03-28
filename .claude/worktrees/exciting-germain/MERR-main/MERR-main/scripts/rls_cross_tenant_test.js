/**
 * ============================================
 * RLS CROSS-TENANT SECURITY TEST (API-level)
 * ============================================
 * Tests that RLS policies correctly isolate
 * data between orchards. No browser needed.
 *
 * Usage: node scripts/rls_cross_tenant_test.js
 * ============================================
 */

import { createClient } from '@supabase/supabase-js';
import { readFileSync } from 'fs';
import { resolve } from 'path';

// Load .env.local
try {
    const envPath = resolve(process.cwd(), '.env.local');
    const envContent = readFileSync(envPath, 'utf-8');
    envContent.split('\n').forEach(line => {
        const [key, ...vals] = line.split('=');
        if (key && !key.startsWith('#') && key.trim()) {
            process.env[key.trim()] = vals.join('=').trim();
        }
    });
} catch { /* skip */ }

const SUPABASE_URL = process.env.VITE_SUPABASE_URL;
const SUPABASE_KEY = process.env.VITE_SUPABASE_ANON_KEY;

if (!SUPABASE_URL || !SUPABASE_KEY) {
    console.error('MISSING: VITE_SUPABASE_URL or VITE_SUPABASE_ANON_KEY');
    process.exit(1);
}

// ============================================
// Test state
// ============================================
const results = [];
let passed = 0;
let failed = 0;

function check(name, condition, detail = '') {
    if (condition) {
        results.push({ name, status: 'PASS' });
        passed++;
        console.log(`  PASS: ${name}`);
    } else {
        results.push({ name, status: 'FAIL', detail });
        failed++;
        console.log(`  FAIL: ${name} ${detail ? '-- ' + detail : ''}`);
    }
}

// ============================================
// ACID user credentials
// ============================================
const MANAGER_A = { email: 'acid_manager_1_1@harvestpro.test', password: 'AcidTest2026!' };
const MANAGER_B = { email: 'acid_manager_2_1@harvestpro.test', password: 'AcidTest2026!' };
const RUNNER_A = { email: 'acid_runner_1_3@harvestpro.test', password: 'AcidTest2026!' };

// Orchard UUIDs (from seed)
const ORCHARD_A = 'a1000000-0000-0000-0000-000000000001';
const ORCHARD_B = 'a1000000-0000-0000-0000-000000000002';

console.log('\n========================================');
console.log('  RLS CROSS-TENANT SECURITY TEST');
console.log('========================================\n');

// ============================================
// Helper: create authenticated client
// ============================================
async function authClient(creds) {
    const client = createClient(SUPABASE_URL, SUPABASE_KEY);
    const { data, error } = await client.auth.signInWithPassword(creds);
    if (error) throw new Error(`Login failed for ${creds.email}: ${error.message}`);
    return { client, user: data.user };
}

try {
    // Login both managers
    console.log('Logging in Manager A (Orchard 1)...');
    const { client: clientA, user: userA } = await authClient(MANAGER_A);
    console.log(`  OK: ${userA.email} (${userA.id})\n`);

    console.log('Logging in Manager B (Orchard 2)...');
    const { client: clientB, user: userB } = await authClient(MANAGER_B);
    console.log(`  OK: ${userB.email} (${userB.id})\n`);

    console.log('Logging in Runner A (Orchard 1)...');
    const { client: clientR, user: userR } = await authClient(RUNNER_A);
    console.log(`  OK: ${userR.email} (${userR.id})\n`);

    // ============================================
    // TEST 1: Manager A cannot see Orchard B pickers
    // ============================================
    console.log('--- TEST 1: SELECT isolation (pickers) ---');
    {
        const { data: pickersA } = await clientA.from('pickers')
            .select('id, orchard_id')
            .eq('orchard_id', ORCHARD_B);

        check(
            'Manager A cannot see Orchard B pickers',
            !pickersA || pickersA.length === 0,
            `Got ${pickersA?.length || 0} rows (expected 0)`
        );
    }

    // ============================================
    // TEST 2: Manager A cannot see Orchard B bucket_records
    // ============================================
    console.log('\n--- TEST 2: SELECT isolation (bucket_records) ---');
    {
        const { data: bucketsA } = await clientA.from('bucket_records')
            .select('id, orchard_id')
            .eq('orchard_id', ORCHARD_B)
            .limit(5);

        check(
            'Manager A cannot see Orchard B bucket_records',
            !bucketsA || bucketsA.length === 0,
            `Got ${bucketsA?.length || 0} rows (expected 0)`
        );
    }

    // ============================================
    // TEST 3: Manager A cannot see Orchard B attendance
    // ============================================
    console.log('\n--- TEST 3: SELECT isolation (attendance) ---');
    {
        const { data: attendanceA } = await clientA.from('attendance')
            .select('id, orchard_id')
            .eq('orchard_id', ORCHARD_B)
            .limit(5);

        check(
            'Manager A cannot see Orchard B attendance',
            !attendanceA || attendanceA.length === 0,
            `Got ${attendanceA?.length || 0} rows (expected 0)`
        );
    }

    // ============================================
    // TEST 4: IDOR - Manager A cannot INSERT bucket into Orchard B
    // ============================================
    console.log('\n--- TEST 4: IDOR INSERT (bucket_records) ---');
    {
        // Get a picker from Orchard B to use as target
        const { data: pickerB } = await clientB.from('pickers')
            .select('id')
            .eq('orchard_id', ORCHARD_B)
            .limit(1)
            .single();

        if (pickerB) {
            const { error: insertErr } = await clientA.from('bucket_records').insert({
                picker_id: pickerB.id,
                orchard_id: ORCHARD_B,  // Attacker tries foreign orchard
                scanned_by: userA.id,
                current_row: 1,
                scanned_at: new Date().toISOString(),
            });

            check(
                'IDOR: Manager A cannot INSERT bucket into Orchard B',
                !!insertErr,
                insertErr ? `Blocked: ${insertErr.message}` : 'INSERT SUCCEEDED (VULNERABILITY!)'
            );
        } else {
            check('IDOR: Manager A cannot INSERT bucket into Orchard B', false, 'No picker found in Orchard B to test with');
        }
    }

    // ============================================
    // TEST 5: IDOR - Runner A cannot INSERT bucket into Orchard B
    // ============================================
    console.log('\n--- TEST 5: IDOR INSERT via Runner (bucket_records) ---');
    {
        const { data: pickerB } = await clientB.from('pickers')
            .select('id')
            .eq('orchard_id', ORCHARD_B)
            .limit(1)
            .single();

        if (pickerB) {
            const { error: insertErr } = await clientR.from('bucket_records').insert({
                picker_id: pickerB.id,
                orchard_id: ORCHARD_B,  // Runner tries foreign orchard
                scanned_by: userR.id,
                current_row: 1,
                scanned_at: new Date().toISOString(),
            });

            check(
                'IDOR: Runner A cannot INSERT bucket into Orchard B',
                !!insertErr,
                insertErr ? `Blocked: ${insertErr.message}` : 'INSERT SUCCEEDED (VULNERABILITY!)'
            );
        } else {
            check('IDOR: Runner A cannot INSERT bucket into Orchard B', false, 'No picker found in Orchard B');
        }
    }

    // ============================================
    // TEST 6: Manager A cannot INSERT picker into Orchard B
    // ============================================
    console.log('\n--- TEST 6: IDOR INSERT (pickers) ---');
    {
        const { error: insertErr } = await clientA.from('pickers').insert({
            full_name: 'IDOR Ghost Picker',
            orchard_id: ORCHARD_B,
            is_active: true,
        });

        check(
            'IDOR: Manager A cannot INSERT picker into Orchard B',
            !!insertErr,
            insertErr ? `Blocked: ${insertErr.message}` : 'INSERT SUCCEEDED (VULNERABILITY!)'
        );
    }

    // ============================================
    // TEST 7: Manager B can see their OWN data
    // ============================================
    console.log('\n--- TEST 7: Positive control (Manager B sees own data) ---');
    {
        const { data: ownPickers } = await clientB.from('pickers')
            .select('id')
            .eq('orchard_id', ORCHARD_B)
            .limit(5);

        check(
            'Positive: Manager B CAN see Orchard B pickers',
            ownPickers && ownPickers.length > 0,
            `Got ${ownPickers?.length || 0} rows`
        );
    }

} catch (err) {
    console.error('\nFATAL ERROR:', err.message);
    process.exit(1);
}

// ============================================
// Summary
// ============================================
console.log('\n========================================');
console.log(`  RESULTS: ${passed} passed, ${failed} failed`);
console.log('========================================');

if (failed > 0) {
    console.log('\n  FAILURES:');
    results.filter(r => r.status === 'FAIL').forEach(r => {
        console.log(`    ${r.name}`);
        if (r.detail) console.log(`      ${r.detail}`);
    });
    console.log('\n  VERDICT: SECURITY ISSUES DETECTED');
    process.exit(1);
} else {
    console.log('\n  VERDICT: ALL RLS POLICIES SECURE');
    process.exit(0);
}
