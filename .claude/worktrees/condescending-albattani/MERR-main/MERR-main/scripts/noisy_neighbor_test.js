/**
 * ============================================
 * HARVESTPRO - NOISY NEIGHBOR TEST
 * ============================================
 * Simulates the "Noisy Neighbor" problem:
 * Manager A runs a heavy date-range payroll report across 500K rows.
 * Manager B simultaneously tries to load their dashboard.
 *
 * The test measures whether Manager A's heavy query degrades
 * Manager B's experience by more than 20%.
 *
 * Usage:
 *   node scripts/noisy_neighbor_test.js
 *
 * Requirements:
 *   - .env with VITE_SUPABASE_URL and VITE_SUPABASE_ANON_KEY
 *   - seed_scale_test.sql already executed on DB
 *
 * Pass Criteria:
 *   - Manager B degradation ≤ 20%
 * ============================================
 */

import { createClient } from '@supabase/supabase-js';
import { readFileSync } from 'fs';
import { resolve } from 'path';

// Load .env.local manually (no dotenv dependency)
try {
    const envPath = resolve(process.cwd(), '.env.local');
    const envContent = readFileSync(envPath, 'utf-8');
    envContent.split('\n').forEach(line => {
        const [key, ...vals] = line.split('=');
        if (key && !key.startsWith('#')) {
            process.env[key.trim()] = vals.join('=').trim();
        }
    });
} catch { /* .env.local not found, use existing env */ }

const SUPABASE_URL = process.env.VITE_SUPABASE_URL;
const SUPABASE_KEY = process.env.VITE_SUPABASE_ANON_KEY;

if (!SUPABASE_URL || !SUPABASE_KEY) {
    console.error('❌ Missing VITE_SUPABASE_URL or VITE_SUPABASE_ANON_KEY in .env');
    process.exit(1);
}

// ============================================
// CONFIG
// ============================================
const ORCHARD_A = 'a1000000-0000-0000-0000-000000000001'; // ACID_ALPHA (heavy query)
const ORCHARD_B = 'a1000000-0000-0000-0000-000000000002'; // ACID_BRAVO (dashboard)
const DATE_RANGE_DAYS = 90; // "Give me 3 months of data"
const ITERATIONS = 5;       // Run multiple times for statistical significance

// ============================================
// QUERIES
// ============================================

/**
 * Heavy query: Manager A exports payroll report
 * This is a cross-table join across bucket_records + daily_attendance + pickers
 * for the full date range. On 500K rows without composite index, this kills CPU.
 */
async function heavyPayrollQuery(supabase) {
    const threeMonthsAgo = new Date();
    threeMonthsAgo.setDate(threeMonthsAgo.getDate() - DATE_RANGE_DAYS);

    const start = performance.now();

    // Heavy query: all bucket records for 3 months, grouped by picker
    const { data, error } = await supabase
        .from('bucket_records')
        .select('picker_id, scanned_at')
        .eq('orchard_id', ORCHARD_A)
        .gte('scanned_at', threeMonthsAgo.toISOString())
        .order('scanned_at', { ascending: false })
        .limit(10000); // Limit to avoid memory crash, but the SCAN is still full

    const duration = performance.now() - start;

    if (error) {
        return { duration, error: error.message, rows: 0 };
    }

    return { duration, error: null, rows: data?.length || 0 };
}

/**
 * Light query: Manager B loads dashboard
 * Simple count + recent 10 records. Should be fast.
 */
async function lightDashboardQuery(supabase) {
    const start = performance.now();

    // Dashboard: count of today's bucket records
    const { count, error: countError } = await supabase
        .from('bucket_records')
        .select('*', { count: 'exact', head: true })
        .eq('orchard_id', ORCHARD_B);

    // Dashboard: last 10 records
    const { data, error: dataError } = await supabase
        .from('bucket_records')
        .select('id, picker_id, scanned_at')
        .eq('orchard_id', ORCHARD_B)
        .order('scanned_at', { ascending: false })
        .limit(10);

    const duration = performance.now() - start;
    const error = countError || dataError;

    return { duration, error: error?.message || null, rows: data?.length || 0, count };
}

// ============================================
// TEST RUNNER
// ============================================

async function runNoisyNeighborTest() {
    console.log('🏠 NOISY NEIGHBOR TEST');
    console.log('='.repeat(60));
    console.log(`  Orchard A (heavy): ${ORCHARD_A} — 3-month payroll export`);
    console.log(`  Orchard B (light): ${ORCHARD_B} — dashboard load`);
    console.log(`  Iterations:        ${ITERATIONS}`);
    console.log('='.repeat(60));

    const supabaseA = createClient(SUPABASE_URL, SUPABASE_KEY);
    const supabaseB = createClient(SUPABASE_URL, SUPABASE_KEY);

    // Phase 1: BASELINE — Manager B alone (no load)
    console.log('\n📊 Phase 1: BASELINE (Manager B alone)...');
    const baselineTimes = [];

    for (let i = 0; i < ITERATIONS; i++) {
        const result = await lightDashboardQuery(supabaseB);
        baselineTimes.push(result.duration);
        console.log(`  Iteration ${i + 1}: ${Math.round(result.duration)}ms (${result.count} total records)`);
    }

    const baselineAvg = baselineTimes.reduce((a, b) => a + b, 0) / baselineTimes.length;
    console.log(`  📈 Baseline avg: ${Math.round(baselineAvg)}ms`);

    // Phase 2: UNDER LOAD — Manager A + Manager B simultaneously
    console.log('\n🌪️ Phase 2: UNDER LOAD (Manager A heavy + Manager B simultaneously)...');
    const loadedTimes = [];
    const heavyTimes = [];

    for (let i = 0; i < ITERATIONS; i++) {
        // Launch both simultaneously
        const [heavyResult, lightResult] = await Promise.all([
            heavyPayrollQuery(supabaseA),
            lightDashboardQuery(supabaseB),
        ]);

        heavyTimes.push(heavyResult.duration);
        loadedTimes.push(lightResult.duration);

        console.log(`  Iteration ${i + 1}: B=${Math.round(lightResult.duration)}ms, A=${Math.round(heavyResult.duration)}ms (${heavyResult.rows} rows scanned)`);

        if (heavyResult.error) {
            console.log(`    ⚠️ Heavy query error: ${heavyResult.error}`);
        }
    }

    const loadedAvg = loadedTimes.reduce((a, b) => a + b, 0) / loadedTimes.length;
    const heavyAvg = heavyTimes.reduce((a, b) => a + b, 0) / heavyTimes.length;

    // ============================================
    // RESULTS
    // ============================================
    const degradation = ((loadedAvg - baselineAvg) / baselineAvg) * 100;

    console.log('\n' + '='.repeat(60));
    console.log('📊 RESULTS');
    console.log('='.repeat(60));
    console.log(`  Manager B baseline avg: ${Math.round(baselineAvg)}ms`);
    console.log(`  Manager B under load:   ${Math.round(loadedAvg)}ms`);
    console.log(`  Manager A heavy avg:    ${Math.round(heavyAvg)}ms`);
    console.log(`  📈 Degradation:         ${degradation.toFixed(1)}%`);

    // ============================================
    // VERDICT
    // ============================================
    console.log('\n' + '='.repeat(60));
    console.log('🏁 VERDICT');
    console.log('='.repeat(60));

    const pass = degradation <= 20;
    const icon = pass ? '✅' : '❌';
    console.log(`  ${icon} Manager B degradation: ${degradation.toFixed(1)}% (threshold: ≤20%)`);

    if (!pass) {
        console.log('\n  💡 RECOMMENDATION:');
        console.log('    Missing composite index. Run this SQL:');
        console.log('    CREATE INDEX idx_bucket_records_orchard_scanned');
        console.log('      ON public.bucket_records(orchard_id, scanned_at DESC);');
    }

    console.log(`\n  ${pass ? '🎉 PASSED' : '💀 FAILED'}\n`);
    process.exit(pass ? 0 : 1);
}

// ============================================
// RUN
// ============================================
runNoisyNeighborTest().catch(err => {
    console.error('❌ Unhandled error:', err);
    process.exit(1);
});
