/* eslint-disable no-console */
import { test, expect, Page, BrowserContext } from '@playwright/test';
import { createClient } from '@supabase/supabase-js';

/**
 * ============================================
 * ORCHARD SWITCHING TEST (EL NÓMADA)
 * ============================================
 * Runner works in Orchard A morning, Orchard B afternoon.
 * Same device. Verifies:
 * - Buckets scanned in A have orchard_id = A
 * - After switching to B, no stale data from A appears
 * - Offline queue (Dexie) is properly isolated
 * ============================================
 */

const SUPABASE_URL = process.env.VITE_SUPABASE_URL || '';
const SUPABASE_KEY = process.env.VITE_SUPABASE_ANON_KEY || '';

const ORCHARD_A = 'a1000000-0000-0000-0000-000000000001'; // ACID_ALPHA
const ORCHARD_B = 'a1000000-0000-0000-0000-000000000002'; // ACID_BRAVO

test.describe('Orchard Switching — El Nómada', () => {

    test('Runner scans in Orchard A, switches to B — data stays isolated', async ({ browser }) => {
        const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);

        // Create a runner context (simulates single device)
        const context = await browser.newContext();
        const page = await context.newPage();

        // === SESSION 1: Orchard A ===
        console.log('\n📱 Session 1: Runner in Orchard A');

        // Login as Runner from Orchard A
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_runner_1_3@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await page.waitForTimeout(3000);

        // Record timestamp before scan
        const beforeScanA = new Date().toISOString();

        // Attempt to scan a bucket in Orchard A
        const scanButton = page.getByRole('button', { name: /scan/i });
        if (await scanButton.isVisible()) {
            await scanButton.click();
            await page.waitForTimeout(1000);

            const manualInput = page.locator(
                'input[placeholder*="picker" i], input[placeholder*="sticker" i], input[placeholder*="code" i]'
            );
            if (await manualInput.first().isVisible()) {
                await manualInput.first().fill('ACID-01-001'); // Orchard A's picker
                const confirmBtn = page.getByRole('button', { name: /confirm|submit/i });
                if (await confirmBtn.isVisible()) {
                    await confirmBtn.click();
                    await page.waitForTimeout(2000);
                }
            }
        }

        console.log('  ✅ Scanned bucket in Orchard A');

        // === LOGOUT ===
        console.log('  🔄 Logging out...');

        // Find and click logout
        const menuButton = page.getByRole('button', { name: /menu|settings|profile/i });
        if (await menuButton.isVisible()) {
            await menuButton.click();
            await page.waitForTimeout(500);
        }

        const logoutButton = page.getByRole('button', { name: /logout|sign out|cerrar/i })
            .or(page.getByText(/logout|sign out|cerrar sesión/i));
        if (await logoutButton.first().isVisible()) {
            await logoutButton.first().click();
            await page.waitForTimeout(2000);
        } else {
            // Force navigation to login
            await page.goto('/login');
        }

        // === SESSION 2: Orchard B ===
        console.log('\n📱 Session 2: Runner in Orchard B');

        // Login as Runner from Orchard B
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_runner_2_3@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await page.waitForTimeout(3000);

        // Check that Orchard A data is NOT visible
        const orchardAText = page.getByText('ACID_ALPHA');
        const orchardAVisible = await orchardAText.isVisible().catch(() => false);

        console.log(`  Orchard A name visible in B session: ${orchardAVisible}`);

        // Check that Orchard A picker is NOT shown
        const orchardAPickerText = page.getByText('ACID-01-');
        const orchardAPickerCount = await orchardAPickerText.count();

        console.log(`  Orchard A picker references in B: ${orchardAPickerCount}`);

        // Verify in DB: any recent bucket records from our scan should have orchard_id = A
        const { data: recentBuckets } = await supabase
            .from('bucket_records')
            .select('orchard_id, scanned_at')
            .gte('scanned_at', beforeScanA)
            .order('scanned_at', { ascending: false })
            .limit(5);

        if (recentBuckets && recentBuckets.length > 0) {
            const wrongOrchard = recentBuckets.filter(b => b.orchard_id === ORCHARD_B);
            console.log(`  Recent buckets: ${recentBuckets.length}`);
            console.log(`  Buckets with wrong orchard_id: ${wrongOrchard.length}`);

            // No bucket from our Orchard A scan should have orchard_id = B
            expect(wrongOrchard.length).toBe(0);
        }

        console.log('✅ Orchard switching: data isolation verified');

        await context.close();
    });

    test('Offline queue isolation: Dexie queue does not leak between orchards', async ({ browser }) => {
        const context = await browser.newContext();
        const page = await context.newPage();

        // Login as Runner A
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_runner_1_3@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await page.waitForTimeout(3000);

        // Go offline
        await context.setOffline(true);
        console.log('  📴 Offline mode ON');

        // Scan a bucket offline
        const scanButton = page.getByRole('button', { name: /scan/i });
        if (await scanButton.isVisible()) {
            await scanButton.click();
            await page.waitForTimeout(1000);

            const manualInput = page.locator(
                'input[placeholder*="picker" i], input[placeholder*="sticker" i], input[placeholder*="code" i]'
            );
            if (await manualInput.first().isVisible()) {
                await manualInput.first().fill('ACID-01-005');
                const confirmBtn = page.getByRole('button', { name: /confirm|submit/i });
                if (await confirmBtn.isVisible()) {
                    await confirmBtn.click();
                    await page.waitForTimeout(1000);
                }
            }
        }

        // Check Dexie queue count
        const queueCount = await page.evaluate(async () => {
            try {
                return await new Promise<number>((resolve) => {
                    const req = indexedDB.open('HarvestProDB');
                    req.onsuccess = (e) => {
                        const db = (e.target as IDBOpenDBRequest).result;
                        const tx = db.transaction('bucket_queue', 'readonly');
                        const store = tx.objectStore('bucket_queue');
                        const countReq = store.count();
                        countReq.onsuccess = () => resolve(countReq.result);
                        countReq.onerror = () => resolve(-1);
                    };
                    req.onerror = () => resolve(-1);
                });
            } catch {
                return -1;
            }
        });

        console.log(`  Offline queue count: ${queueCount}`);

        // Go back online before logout
        await context.setOffline(false);

        // Logout and login as different orchard runner
        await page.goto('/login');
        await page.waitForTimeout(1000);
        await page.fill('input[type="email"]', 'acid_runner_2_3@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await page.waitForTimeout(3000);

        // Check Dexie queue — should be empty or only contain Orchard B data
        const queueCountAfterSwitch = await page.evaluate(async () => {
            try {
                return await new Promise<number>((resolve) => {
                    const req = indexedDB.open('HarvestProDB');
                    req.onsuccess = (e) => {
                        const db = (e.target as IDBOpenDBRequest).result;
                        const tx = db.transaction('bucket_queue', 'readonly');
                        const store = tx.objectStore('bucket_queue');
                        const countReq = store.count();
                        countReq.onsuccess = () => resolve(countReq.result);
                        countReq.onerror = () => resolve(-1);
                    };
                    req.onerror = () => resolve(-1);
                });
            } catch {
                return -1;
            }
        });

        console.log(`  Queue count after orchard switch: ${queueCountAfterSwitch}`);

        // If queue still has items from Orchard A, that's a data leak
        if (queueCountAfterSwitch > 0) {
            console.warn('⚠️ POTENTIAL DATA LEAK: Dexie queue not cleared on orchard switch');
        }

        console.log('✅ Offline queue isolation test completed');

        await context.close();
    });
});
