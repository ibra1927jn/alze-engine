/**
 * ============================================
 * HARVESTPRO - CONCURRENCY STRESS TEST
 * ============================================
 * Simulates "Bad WiFi Batch Sync" — the real-world scenario where
 * 10 devices come back online simultaneously and each flushes
 * 100 queued bucket records at once.
 *
 * Usage:
 *   node scripts/concurrency_test.js
 *
 * Requirements:
 *   - .env with VITE_SUPABASE_URL and VITE_SUPABASE_ANON_KEY
 *   - seed_scale_test.sql already executed on DB
 *
 * Pass Criteria:
 *   - 0 errors 500
 *   - 0 duplicate rows
 *   - p95 response < 3s
 *   - No deadlocks
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
const NUM_DEVICES = 10;             // Concurrent "devices" syncing
const BUCKETS_PER_DEVICE = 100;     // Each device queued 100 buckets offline
const JITTER_WINDOW_MS = 500;       // Requests arrive within 500ms window
const ORCHARD_ID = 'a1000000-0000-0000-0000-000000000001'; // ACID_ALPHA

// Deterministic picker UUIDs from seed (orchard 1, pickers 1-10)
const PICKER_IDS = Array.from({ length: 10 }, (_, i) =>
    `c1000000-0000-0001-0000-0000000000${String(i + 1).padStart(2, '0')}`
);

// Deterministic runner UUIDs from seed (orchard 1, runners 3-5)
const RUNNER_IDS = [
    'b1000000-0000-0000-0001-000000000003',
    'b1000000-0000-0000-0001-000000000004',
    'b1000000-0000-0000-0001-000000000005',
];

// ============================================
// HELPERS
// ============================================

function generateBatchPayload(deviceIndex) {
    const runnerId = RUNNER_IDS[deviceIndex % RUNNER_IDS.length];
    const records = [];

    for (let i = 0; i < BUCKETS_PER_DEVICE; i++) {
        const pickerIdx = (deviceIndex * BUCKETS_PER_DEVICE + i) % PICKER_IDS.length;
        const scanTime = new Date(Date.now() - Math.random() * 7200000); // Random within last 2 hours

        records.push({
            orchard_id: ORCHARD_ID,
            picker_id: PICKER_IDS[pickerIdx],
            scanned_by: runnerId,
            scanned_at: scanTime.toISOString(),
            coords: {
                lat: -39.5 + Math.random() * 0.1,
                lng: 176.8 + Math.random() * 0.1,
            },
        });
    }

    return records;
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function percentile(arr, p) {
    const sorted = [...arr].sort((a, b) => a - b);
    const idx = Math.ceil((p / 100) * sorted.length) - 1;
    return sorted[Math.max(0, idx)];
}

// ============================================
// DEVICE SIMULATOR
// ============================================

async function simulateDeviceSync(deviceIndex) {
    // Each device gets its own client (simulates separate connections)
    const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);

    const payload = generateBatchPayload(deviceIndex);
    const startTime = performance.now();

    try {
        const { data, error } = await supabase
            .from('bucket_records')
            .insert(payload);

        const duration = performance.now() - startTime;

        if (error) {
            return {
                device: deviceIndex,
                success: false,
                duration,
                error: error.message,
                code: error.code,
                rows: 0,
            };
        }

        return {
            device: deviceIndex,
            success: true,
            duration,
            error: null,
            code: null,
            rows: BUCKETS_PER_DEVICE,
        };
    } catch (err) {
        const duration = performance.now() - startTime;
        return {
            device: deviceIndex,
            success: false,
            duration,
            error: err.message,
            code: 'EXCEPTION',
            rows: 0,
        };
    }
}

// ============================================
// MAIN TEST RUNNER
// ============================================

async function runConcurrencyTest() {
    console.log('🌪️  CONCURRENCY STRESS TEST — "Bad WiFi Batch Sync"');
    console.log('='.repeat(60));
    console.log(`  Devices:        ${NUM_DEVICES}`);
    console.log(`  Buckets/device: ${BUCKETS_PER_DEVICE}`);
    console.log(`  Total inserts:  ${NUM_DEVICES * BUCKETS_PER_DEVICE}`);
    console.log(`  Jitter window:  ${JITTER_WINDOW_MS}ms`);
    console.log(`  Target orchard: ${ORCHARD_ID}`);
    console.log('='.repeat(60));

    // Count records before
    const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);
    const { count: beforeCount } = await supabase
        .from('bucket_records')
        .select('*', { count: 'exact', head: true })
        .eq('orchard_id', ORCHARD_ID);

    console.log(`\n📊 Records before: ${beforeCount}`);
    console.log('\n🚀 Launching devices with jitter...\n');

    // Launch all devices with random jitter
    const promises = [];
    const launchTimes = [];

    for (let i = 0; i < NUM_DEVICES; i++) {
        const jitter = Math.random() * JITTER_WINDOW_MS;
        launchTimes.push(jitter);

        const delayed = sleep(jitter).then(() => {
            console.log(`  📱 Device ${i} syncing (${Math.round(jitter)}ms jitter)...`);
            return simulateDeviceSync(i);
        });

        promises.push(delayed);
    }

    const overallStart = performance.now();
    const results = await Promise.all(promises);
    const overallDuration = performance.now() - overallStart;

    // ============================================
    // RESULTS ANALYSIS
    // ============================================
    console.log('\n' + '='.repeat(60));
    console.log('📊 RESULTS');
    console.log('='.repeat(60));

    const successes = results.filter(r => r.success);
    const failures = results.filter(r => !r.success);
    const durations = results.map(r => r.duration);

    console.log(`\n  ✅ Successful:  ${successes.length}/${NUM_DEVICES}`);
    console.log(`  ❌ Failed:      ${failures.length}/${NUM_DEVICES}`);
    console.log(`  ⏱️  Total time:  ${Math.round(overallDuration)}ms`);
    console.log(`  📈 p50 latency: ${Math.round(percentile(durations, 50))}ms`);
    console.log(`  📈 p95 latency: ${Math.round(percentile(durations, 95))}ms`);
    console.log(`  📈 p99 latency: ${Math.round(percentile(durations, 99))}ms`);
    console.log(`  📈 Max latency: ${Math.round(Math.max(...durations))}ms`);

    if (failures.length > 0) {
        console.log('\n  ❌ FAILURES:');
        failures.forEach(f => {
            console.log(`    Device ${f.device}: ${f.error} (code: ${f.code})`);
        });
    }

    // Count records after
    const { count: afterCount } = await supabase
        .from('bucket_records')
        .select('*', { count: 'exact', head: true })
        .eq('orchard_id', ORCHARD_ID);

    const inserted = afterCount - beforeCount;
    const expected = successes.length * BUCKETS_PER_DEVICE;

    console.log(`\n  📊 Records after:   ${afterCount}`);
    console.log(`  📊 Actually inserted: ${inserted}`);
    console.log(`  📊 Expected inserted: ${expected}`);

    // ============================================
    // PASS/FAIL VERDICT
    // ============================================
    console.log('\n' + '='.repeat(60));
    console.log('🏁 VERDICT');
    console.log('='.repeat(60));

    const p95 = percentile(durations, 95);
    const has500 = failures.some(f => f.code === '500' || f.error?.includes('500'));
    const hasDeadlock = failures.some(f =>
        f.error?.toLowerCase().includes('deadlock') ||
        f.error?.toLowerCase().includes('lock timeout')
    );

    const checks = [
        { name: 'No 500 errors', pass: !has500 },
        { name: 'No deadlocks', pass: !hasDeadlock },
        { name: 'p95 < 3000ms', pass: p95 < 3000 },
        { name: 'All devices succeeded', pass: failures.length === 0 },
        { name: 'Row count matches', pass: inserted === expected },
    ];

    let allPass = true;
    checks.forEach(c => {
        const icon = c.pass ? '✅' : '❌';
        console.log(`  ${icon} ${c.name}`);
        if (!c.pass) allPass = false;
    });

    console.log(`\n  ${allPass ? '🎉 ALL CHECKS PASSED' : '💀 SOME CHECKS FAILED'}\n`);

    process.exit(allPass ? 0 : 1);
}

// ============================================
// RUN
// ============================================
runConcurrencyTest().catch(err => {
    console.error('❌ Unhandled error:', err);
    process.exit(1);
});
