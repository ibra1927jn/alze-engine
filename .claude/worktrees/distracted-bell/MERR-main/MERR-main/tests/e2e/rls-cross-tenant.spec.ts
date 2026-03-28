/* eslint-disable no-console */
import { test, expect, Page } from '@playwright/test';
import { createClient, SupabaseClient } from '@supabase/supabase-js';

/**
 * ============================================
 * CROSS-TENANT RLS SECURITY TEST + IDOR ATTACK
 * ============================================
 * Tests that data from Orchard B is NEVER visible to Manager A.
 * Includes IDOR INSERT attack: can Manager A insert data into Orchard B?
 *
 * Uses ACID test orchards from seed_scale_test.sql
 * ============================================
 */

const SUPABASE_URL = process.env.VITE_SUPABASE_URL || '';
const SUPABASE_KEY = process.env.VITE_SUPABASE_ANON_KEY || '';

// ACID Orchards
const ORCHARD_A = 'a1000000-0000-0000-0000-000000000001'; // ACID_ALPHA
const ORCHARD_B = 'a1000000-0000-0000-0000-000000000002'; // ACID_BRAVO

// SPY picker — exists in Orchard B only
const SPY_PICKER_ID = 'SPY-CROSS-TENANT-001';
const SPY_PICKER_NAME = 'ESPIA Secreto';

test.describe('Cross-Tenant RLS Security', () => {
    let supabaseAdmin: SupabaseClient;

    test.beforeAll(async () => {
        supabaseAdmin = createClient(SUPABASE_URL, SUPABASE_KEY);

        // Insert SPY picker into Orchard B
        await supabaseAdmin.from('pickers').upsert({
            picker_id: SPY_PICKER_ID,
            name: SPY_PICKER_NAME,
            orchard_id: ORCHARD_B,
            status: 'active',
            safety_verified: true,
        }, { onConflict: 'picker_id' });

        // Insert SPY bucket record in Orchard B
        await supabaseAdmin.from('bucket_records').insert({
            orchard_id: ORCHARD_B,
            scanned_at: new Date().toISOString(),
            scanned_by: 'b1000000-0000-0000-0002-000000000003', // Orchard B's runner
            coords: { lat: -39.5, lng: 176.8 },
        });
    });

    test('CRITICAL: Manager A cannot see Orchard B pickers (SELECT blocked)', async ({ page }) => {
        // Login as Manager A
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_manager_1_1@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/manager/, { timeout: 15000 });

        // Navigate to Teams/Personnel
        await page.getByRole('link', { name: /personnel|team/i }).click();
        await page.waitForTimeout(2000);

        // SPY picker should NOT be visible
        const spyElement = page.getByText(SPY_PICKER_NAME);
        await expect(spyElement).not.toBeVisible();

        // Also search for SPY picker ID
        const searchInput = page.locator('input[placeholder*="search" i]');
        if (await searchInput.isVisible()) {
            await searchInput.fill(SPY_PICKER_ID);
            await page.waitForTimeout(1000);
            await expect(spyElement).not.toBeVisible();
        }

        console.log('✅ Manager A cannot see Orchard B pickers');
    });

    test('CRITICAL: Manager A cannot see Orchard B bucket records (SELECT blocked)', async ({ page }) => {
        // Login as Manager A
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_manager_1_1@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/manager/, { timeout: 15000 });

        // Use Supabase client with Manager A's session to query bucket_records
        const token = await page.evaluate(() => {
            const sessionStr = localStorage.getItem('sb-' + location.hostname.split('.')[0] + '-auth-token');
            if (sessionStr) {
                const session = JSON.parse(sessionStr);
                return session?.access_token;
            }
            return null;
        });

        if (token) {
            const supabaseAsManagerA = createClient(SUPABASE_URL, SUPABASE_KEY, {
                global: { headers: { Authorization: `Bearer ${token}` } },
            });

            // Try to read Orchard B's bucket records
            const { data: records, error } = await supabaseAsManagerA
                .from('bucket_records')
                .select('*')
                .eq('orchard_id', ORCHARD_B)
                .limit(10);

            // Should get 0 records (RLS blocks) or an error
            expect(records?.length || 0).toBe(0);
            console.log('✅ Manager A cannot see Orchard B bucket records');
        }
    });

    test('CRITICAL: IDOR — Manager A cannot INSERT bucket into Orchard B', async ({ page }) => {
        // Login as Manager A
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_manager_1_1@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/manager/, { timeout: 15000 });

        // Get Manager A's JWT
        const token = await page.evaluate(() => {
            const keys = Object.keys(localStorage);
            const authKey = keys.find(k => k.includes('auth-token'));
            if (authKey) {
                const session = JSON.parse(localStorage.getItem(authKey) || '{}');
                return session?.access_token;
            }
            return null;
        });

        expect(token).toBeTruthy();

        if (token) {
            const supabaseAsManagerA = createClient(SUPABASE_URL, SUPABASE_KEY, {
                global: { headers: { Authorization: `Bearer ${token}` } },
            });

            // IDOR ATTACK: Try to INSERT a bucket record into Orchard B
            const { data, error } = await supabaseAsManagerA
                .from('bucket_records')
                .insert({
                    orchard_id: ORCHARD_B,  // ← FOREIGN ORCHARD
                    picker_id: 'c1000000-0000-0002-0000-000000000001', // Orchard B's picker
                    scanned_at: new Date().toISOString(),
                    coords: { lat: -39.5, lng: 176.8 },
                });

            // This MUST fail. If it succeeds, we have a critical IDOR vulnerability.
            if (!error) {
                console.error('💀 CRITICAL IDOR VULNERABILITY: Manager A inserted into Orchard B!');
            }
            expect(error).toBeTruthy();
            console.log(`✅ IDOR blocked: ${error?.message || 'INSERT rejected'}`);
        }
    });

    test('CRITICAL: IDOR — Manager A cannot INSERT picker into Orchard B', async ({ page }) => {
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_manager_1_1@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/manager/, { timeout: 15000 });

        const token = await page.evaluate(() => {
            const keys = Object.keys(localStorage);
            const authKey = keys.find(k => k.includes('auth-token'));
            if (authKey) {
                const session = JSON.parse(localStorage.getItem(authKey) || '{}');
                return session?.access_token;
            }
            return null;
        });

        if (token) {
            const supabaseAsManagerA = createClient(SUPABASE_URL, SUPABASE_KEY, {
                global: { headers: { Authorization: `Bearer ${token}` } },
            });

            // IDOR: Try to add a picker to Orchard B
            const { error } = await supabaseAsManagerA
                .from('pickers')
                .insert({
                    picker_id: 'IDOR-ATTACK-001',
                    name: 'IDOR Attacker',
                    orchard_id: ORCHARD_B, // ← FOREIGN ORCHARD
                    status: 'active',
                });

            // This MUST fail
            if (!error) {
                console.error('💀 CRITICAL: Manager A added picker to Orchard B!');
                // Clean up rogue picker
                await supabaseAsManagerA.from('pickers').delete().eq('picker_id', 'IDOR-ATTACK-001');
            }
            expect(error).toBeTruthy();
            console.log(`✅ IDOR picker INSERT blocked: ${error?.message || 'rejected'}`);
        }
    });

    test('CRITICAL: IDOR — Manager A cannot UPDATE Orchard B attendance', async ({ page }) => {
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_manager_1_1@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/manager/, { timeout: 15000 });

        const token = await page.evaluate(() => {
            const keys = Object.keys(localStorage);
            const authKey = keys.find(k => k.includes('auth-token'));
            if (authKey) {
                const session = JSON.parse(localStorage.getItem(authKey) || '{}');
                return session?.access_token;
            }
            return null;
        });

        if (token) {
            const supabaseAsManagerA = createClient(SUPABASE_URL, SUPABASE_KEY, {
                global: { headers: { Authorization: `Bearer ${token}` } },
            });

            // Try to update Orchard B's attendance records
            const { data, error } = await supabaseAsManagerA
                .from('daily_attendance')
                .update({ status: 'absent' })
                .eq('orchard_id', ORCHARD_B)
                .select();

            // Should update 0 rows (RLS blocks) or return error
            const rowsAffected = data?.length || 0;
            expect(rowsAffected).toBe(0);
            console.log(`✅ IDOR attendance UPDATE blocked (${rowsAffected} rows affected)`);
        }
    });

    test.afterAll(async () => {
        // Clean up SPY picker
        if (supabaseAdmin) {
            await supabaseAdmin.from('pickers').delete().eq('picker_id', SPY_PICKER_ID);
        }
    });
});
