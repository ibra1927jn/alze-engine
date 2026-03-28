/**
 * ============================================
 * HARVESTPRO - SEED RUNNER (Authenticated)
 * ============================================
 * Signs in as manager, then inserts test data.
 * Uses the authenticated session to pass RLS.
 *
 * Usage: node scripts/seed_runner.js
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
        if (key && !key.startsWith('#')) {
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

const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);

// ============================================
// AUTH: Sign in as existing manager
// ============================================
const { data: authData, error: authError } = await supabase.auth.signInWithPassword({
    email: 'man2@gmail.com',
    password: '111111',
});

if (authError) {
    console.error('LOGIN FAILED:', authError.message);
    process.exit(1);
}

const AUTH_USER_ID = authData.user.id;
console.log('Authenticated as:', authData.user.email, '(' + AUTH_USER_ID + ')');

// Get manager's orchard
const { data: profile } = await supabase.from('users').select('orchard_id').eq('id', AUTH_USER_ID).single();
const MANAGER_ORCHARD = profile?.orchard_id;
console.log('Manager orchard:', MANAGER_ORCHARD);

// ============================================
// DATA GENERATION
// ============================================
const ORCHARDS = Array.from({ length: 10 }, (_, i) => ({
    id: `a1000000-0000-0000-0000-0000000000${String(i + 1).padStart(2, '0')}`,
    name: `ACID_${['ALPHA', 'BRAVO', 'CHARLIE', 'DELTA', 'ECHO', 'FOXTROT', 'GOLF', 'HOTEL', 'INDIA', 'JULIET'][i]}`,
    location: ['Hastings, HB', 'Napier, HB', 'Gisborne', 'Havelock North', 'Meeanee', 'Hastings', 'Hastings', 'Napier', 'Gisborne', 'Central Otago'][i],
    total_blocks: [480, 520, 320, 550, 280, 400, 650, 220, 380, 420][i],
    code: `ACID${String(i + 1).padStart(2, '0')}`,
}));

function generatePickers() {
    const pickers = [];
    for (let oi = 1; oi <= 10; oi++) {
        const orchardId = `a1000000-0000-0000-0000-0000000000${String(oi).padStart(2, '0')}`;
        for (let pi = 1; pi <= 20; pi++) {
            pickers.push({
                id: `c1000000-0000-${String(oi).padStart(4, '0')}-0000-0000000000${String(pi).padStart(2, '0')}`,
                picker_id: `ACID-${String(oi).padStart(2, '0')}-${String(pi).padStart(3, '0')}`,
                name: `Picker ${oi}-${pi}`,
                orchard_id: orchardId,
                status: pi <= 18 ? 'active' : pi === 19 ? 'archived' : 'inactive',
                safety_verified: pi <= 16,
                current_row: (pi % 15) + 1,
            });
        }
    }
    return pickers;
}

function generateAttendance(pickers) {
    const records = [];
    const now = new Date();
    for (const picker of pickers) {
        if (picker.status !== 'active') continue;
        for (let dayOffset = 1; dayOffset <= 90; dayOffset++) {
            const date = new Date(now);
            date.setDate(date.getDate() - dayOffset);
            if (date.getDay() === 0) continue;
            const dateStr = date.toISOString().split('T')[0];
            const rand = Math.random();
            const checkIn = new Date(date);
            checkIn.setHours(6 + Math.floor(Math.random() * 2), Math.floor(Math.random() * 60));
            const checkOut = rand < 0.05 ? null : (() => {
                const co = new Date(date);
                co.setHours(15 + Math.floor(Math.random() * 3), Math.floor(Math.random() * 60));
                return co.toISOString();
            })();
            records.push({
                picker_id: picker.id,
                orchard_id: picker.orchard_id,
                date: dateStr,
                check_in_time: checkIn.toISOString(),
                check_out_time: checkOut,
                status: rand < 0.03 ? 'absent' : rand < 0.05 ? 'late' : 'present',
            });
        }
    }
    return records;
}

// ============================================
// BATCH INSERT HELPER
// ============================================
async function insertBatch(table, records, batchSize = 500) {
    let inserted = 0;
    let errors = 0;
    for (let i = 0; i < records.length; i += batchSize) {
        const batch = records.slice(i, i + batchSize);
        const { data, error } = await supabase.from(table).upsert(batch, { ignoreDuplicates: true });
        if (error) {
            if (errors < 3) console.log(`  [${table}] error: ${error.message}`);
            errors++;
        } else {
            inserted += batch.length;
        }
    }
    if (errors > 3) console.log(`  [${table}] ... and ${errors - 3} more errors`);
    return inserted;
}

// ============================================
// MAIN
// ============================================
async function main() {
    console.log('\nACID SEED RUNNER - Starting...');
    console.log('========================================');
    const t0 = Date.now();

    // Step 1: Orchards
    console.log('\n[1/4] Orchards...');
    const oc = await insertBatch('orchards', ORCHARDS);
    console.log(`  Inserted: ${oc}`);

    // Step 2: Pickers (manager RLS allows ALL for managers)
    console.log('\n[2/4] Pickers...');
    const pickers = generatePickers();
    const pc = await insertBatch('pickers', pickers);
    console.log(`  Inserted: ${pc}`);

    // Step 3: Attendance
    console.log('\n[3/4] Attendance...');
    const attendance = generateAttendance(pickers);
    console.log(`  Generated: ${attendance.length} records`);
    const ac = await insertBatch('daily_attendance', attendance, 200);
    console.log(`  Inserted: ${ac}`);

    // Step 4: Bucket records
    // RLS: "Runners insert records" requires auth.uid() = scanned_by
    // So we set scanned_by = our auth user ID
    console.log('\n[4/4] Bucket records (batched by day, ~500K target)...');
    let totalBuckets = 0;
    const activePickers = pickers.filter(p => p.status === 'active');

    for (let day = 1; day <= 90; day++) {
        const date = new Date();
        date.setDate(date.getDate() - day);
        if (date.getDay() === 0) continue;

        const batch = [];
        for (const picker of activePickers) {
            if (Math.random() < 0.03) continue; // 3% ghost days
            const count = 15 + Math.floor(Math.random() * 16);
            for (let b = 0; b < count; b++) {
                const scanTime = new Date(date);
                scanTime.setHours(7 + Math.floor(Math.random() * 9), Math.floor(Math.random() * 60), Math.floor(Math.random() * 60));
                batch.push({
                    orchard_id: picker.orchard_id,
                    picker_id: picker.id,
                    scanned_by: AUTH_USER_ID, // Must match auth.uid() for RLS
                    scanned_at: scanTime.toISOString(),
                    coords: { lat: -39.5 + Math.random() * 0.1, lng: 176.8 + Math.random() * 0.1 },
                });
            }
        }

        // Insert in sub-batches of 500
        for (let i = 0; i < batch.length; i += 500) {
            const sub = batch.slice(i, i + 500);
            const { error } = await supabase.from('bucket_records').insert(sub);
            if (error) {
                if (totalBuckets === 0) console.log(`  Error: ${error.message}`);
            } else {
                totalBuckets += sub.length;
            }
        }

        if (day % 15 === 0) {
            console.log(`  Day ${day}/90: ${totalBuckets} buckets so far...`);
        }
    }
    console.log(`  Total: ${totalBuckets}`);

    // Verify
    console.log('\n========================================');
    console.log('VERIFICATION:');
    const { count: oFinal } = await supabase.from('orchards').select('*', { count: 'exact', head: true }).like('name', 'ACID_%');
    const { count: pFinal } = await supabase.from('pickers').select('*', { count: 'exact', head: true }).like('picker_id', 'ACID-%');
    const { count: aFinal } = await supabase.from('daily_attendance').select('*', { count: 'exact', head: true }).in('orchard_id', ORCHARDS.map(o => o.id));
    const { count: bFinal } = await supabase.from('bucket_records').select('*', { count: 'exact', head: true }).in('orchard_id', ORCHARDS.map(o => o.id));

    console.log(`  ORCHARDS:       ${oFinal}`);
    console.log(`  PICKERS:        ${pFinal}`);
    console.log(`  ATTENDANCE:     ${aFinal}`);
    console.log(`  BUCKET RECORDS: ${bFinal}`);

    const elapsed = ((Date.now() - t0) / 1000).toFixed(1);
    console.log(`\nDone in ${elapsed}s`);
}

main().catch(err => { console.error('FATAL:', err.message); process.exit(1); });
