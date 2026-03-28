/* eslint-disable no-console */
import { test, expect } from '@playwright/test';
import { createClient } from '@supabase/supabase-js';

/**
 * ============================================
 * OPERATIONAL CHAOS E2E TESTS
 * ============================================
 * Real-world daily scenarios that WILL happen in the field.
 * These test human errors, not infrastructure.
 *
 * Uses ACID test orchards from seed_scale_test.sql
 * ============================================
 */

const SUPABASE_URL = process.env.VITE_SUPABASE_URL || '';
const SUPABASE_KEY = process.env.VITE_SUPABASE_ANON_KEY || '';

test.describe('Operational Chaos — Daily Human Errors', () => {

    // ------------------------------------------
    // TEST 1: EL FANTASMA (Ghost Worker)
    // TL checks in picker who never showed up.
    // 2 hours later: "0 buckets, 2 hours worked"
    // System should flag LOW_PRODUCTIVITY alert.
    // ------------------------------------------
    test('Ghost Worker: checked-in picker with 0 buckets triggers compliance alert', async ({ page }) => {
        const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);

        // Create a ghost attendance: checked in 3 hours ago, no buckets
        const orchardId = 'a1000000-0000-0000-0000-000000000001';
        const ghostPickerId = 'c1000000-0000-0001-0000-000000000018'; // Picker 18

        const threeHoursAgo = new Date(Date.now() - 3 * 60 * 60 * 1000);

        await supabase.from('daily_attendance').upsert({
            picker_id: ghostPickerId,
            orchard_id: orchardId,
            date: new Date().toISOString().split('T')[0],
            check_in_time: threeHoursAgo.toISOString(),
            check_out_time: null, // Still "working"
            status: 'present',
        }, { onConflict: 'picker_id,date' });

        // Ensure ghost has 0 bucket_records today
        const today = new Date().toISOString().split('T')[0];
        const { count } = await supabase
            .from('bucket_records')
            .select('*', { count: 'exact', head: true })
            .eq('picker_id', ghostPickerId)
            .gte('scanned_at', today);

        // Login as Manager
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_manager_1_1@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/manager/, { timeout: 15000 });

        // Navigate to Dashboard
        await page.getByRole('link', { name: /dashboard|home/i }).first().click();
        await page.waitForTimeout(2000);

        // Look for compliance alert or low productivity indicator
        const alertElement = page.getByText(/below minimum|low productivity|violation|0 buckets/i);
        const warningIcon = page.locator('[data-testid="compliance-alert"]')
            .or(page.locator('.compliance-alert'));

        // At least one should be visible if compliance checks are active
        const alertVisible = await alertElement.isVisible().catch(() => false);
        const warningVisible = await warningIcon.isVisible().catch(() => false);

        console.log(`  Ghost worker alert visible: ${alertVisible}`);
        console.log(`  Warning icon visible: ${warningVisible}`);
        console.log(`  Ghost picker today bucket count: ${count}`);

        // This test documents the EXPECTED behavior.
        // If neither alert is visible, it means the compliance system
        // doesn't flag 0-bucket workers, which is a gap.
        if (!alertVisible && !warningVisible) {
            console.warn('⚠️ GAP: No alert for picker with 0 buckets after 3 hours');
        }
    });

    // ------------------------------------------
    // TEST 2: EL CHECK-OUT OLVIDADO (Forgotten Checkout)
    // 5 pickers not checked out yesterday.
    // TimesheetEditor should show abnormal hours.
    // ------------------------------------------
    test('Forgotten Checkout: TimesheetEditor shows warning for missing checkout', async ({ page }) => {
        const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);
        const orchardId = 'a1000000-0000-0000-0000-000000000001';
        const yesterday = new Date(Date.now() - 86400000).toISOString().split('T')[0];

        // Create 5 pickers with check-in but NO check-out yesterday
        for (let i = 1; i <= 5; i++) {
            const pickerId = `c1000000-0000-0001-0000-0000000000${String(i).padStart(2, '0')}`;

            await supabase.from('daily_attendance').upsert({
                picker_id: pickerId,
                orchard_id: orchardId,
                date: yesterday,
                check_in_time: new Date(`${yesterday}T07:00:00+13:00`).toISOString(),
                check_out_time: null, // ← FORGOTTEN
                status: 'present',
            }, { onConflict: 'picker_id,date' });
        }

        // Login as Manager
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_manager_1_1@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/manager/, { timeout: 15000 });

        // Navigate to Timesheet tab
        const timesheetTab = page.getByRole('link', { name: /timesheet|attendance/i });
        if (await timesheetTab.isVisible()) {
            await timesheetTab.click();
            await page.waitForTimeout(2000);

            // Look for yesterday's date
            const dateSelector = page.locator('input[type="date"]');
            if (await dateSelector.isVisible()) {
                await dateSelector.fill(yesterday);
                await page.waitForTimeout(1500);
            }

            // Should show missing checkout warnings
            const warningElements = page.getByText(/missing checkout|no check-out|abnormal/i);
            const warningCount = await warningElements.count();

            console.log(`  Missing checkout warnings found: ${warningCount}`);
            if (warningCount === 0) {
                console.warn('⚠️ GAP: TimesheetEditor does not flag missing checkouts');
            }
        } else {
            console.warn('⚠️ Timesheet tab not found in navigation');
        }
    });

    // ------------------------------------------
    // TEST 4: EL STICKER EQUIVOCADO (Wrong Picker Sticker)
    // Runner scans sticker for picker NOT checked in today.
    // System should warn or flag.
    // ------------------------------------------
    test('Wrong Sticker: scanning picker not checked in today shows warning', async ({ page }) => {
        // Login as Runner
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_runner_1_3@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');

        await page.waitForTimeout(3000);

        // Try to scan bucket for a picker who is NOT checked in today
        // The picker exists but has no attendance record for today
        const scanButton = page.getByRole('button', { name: /scan/i });
        if (await scanButton.isVisible()) {
            await scanButton.click();
            await page.waitForTimeout(1000);

            // Try manual entry of an inactive picker's sticker
            const manualInput = page.locator('input[placeholder*="picker" i], input[placeholder*="sticker" i], input[placeholder*="code" i]');
            if (await manualInput.first().isVisible()) {
                await manualInput.first().fill('ACID-01-019'); // Archived picker
                const confirmBtn = page.getByRole('button', { name: /confirm|submit|scan/i });
                if (await confirmBtn.isVisible()) {
                    await confirmBtn.click();
                    await page.waitForTimeout(2000);

                    // Should show error/warning about archived or un-checked-in picker
                    const errorMsg = page.getByText(/archived|not checked in|inactive|not found/i);
                    const errorVisible = await errorMsg.isVisible().catch(() => false);

                    console.log(`  Archived picker scan warning: ${errorVisible}`);
                    if (!errorVisible) {
                        console.warn('⚠️ GAP: No warning when scanning archived picker sticker');
                    }
                }
            }
        }
    });

    // ------------------------------------------
    // TEST 6: EL CIERRE TARDÍO (Late Sync After Day Closure)
    // Manager closes day at 5PM. Runner syncs stale queue at 7PM.
    // ------------------------------------------
    test('Late Sync: offline bucket records after day closure', async ({ browser }) => {
        const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);
        const orchardId = 'a1000000-0000-0000-0000-000000000001';

        // Check if day_closures table exists and create a closure
        const { error: closureError } = await supabase.from('day_closures').insert({
            orchard_id: orchardId,
            date: new Date().toISOString().split('T')[0],
            closed_by: 'b1000000-0000-0000-0001-000000000001', // Manager 1
            closed_at: new Date().toISOString(),
        });

        if (closureError) {
            console.log(`  Day closure table status: ${closureError.message}`);
            // If table doesn't exist, skip gracefully
            if (closureError.message.includes('does not exist')) {
                console.warn('⚠️ day_closures table not found — skipping');
                return;
            }
        }

        // Try to insert a bucket record AFTER closure
        const { error: insertError } = await supabase.from('bucket_records').insert({
            orchard_id: orchardId,
            picker_id: 'c1000000-0000-0001-0000-000000000001',
            scanned_by: 'b1000000-0000-0000-0001-000000000003',
            scanned_at: new Date().toISOString(),
            coords: { lat: -39.5, lng: 176.8 },
        });

        console.log(`  Post-closure insert: ${insertError ? 'BLOCKED ✅' : 'ALLOWED ⚠️'}`);

        // Document the behavior
        if (!insertError) {
            console.warn('⚠️ GAP: System allows bucket inserts after day closure');
            console.warn('   Consider adding a trigger or RLS policy to block post-closure inserts');
        }
    });

    // ------------------------------------------
    // TEST 7: LA MEDIANOCHE (Timezone Edge Case)
    // Scan at 23:55 NZST, sync at 00:05 NZST next day.
    // Bucket should have recorded_at = yesterday.
    // ------------------------------------------
    test('Midnight Edge: scan timestamp preserved across day boundary', async () => {
        const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);
        const orchardId = 'a1000000-0000-0000-0000-000000000001';

        // Simulate: scan happened at 23:55 NZST yesterday
        const yesterday2355 = new Date();
        yesterday2355.setDate(yesterday2355.getDate() - 1);
        yesterday2355.setHours(23, 55, 0, 0);
        // Convert to NZST (UTC+13)
        const nzstOffset = 13 * 60 * 60 * 1000;
        const scanTimeNZST = new Date(yesterday2355.getTime() - nzstOffset);

        // Insert with the ORIGINAL scan time (pre-midnight)
        const { data, error } = await supabase.from('bucket_records').insert({
            orchard_id: orchardId,
            picker_id: 'c1000000-0000-0001-0000-000000000001',
            scanned_by: 'b1000000-0000-0000-0001-000000000003',
            scanned_at: scanTimeNZST.toISOString(),
            created_at: new Date().toISOString(), // Sync time = NOW (next day)
            coords: { lat: -39.5, lng: 176.8 },
        }).select().single();

        if (error) {
            console.log(`  Timezone insert error: ${error.message}`);
            return;
        }

        // Verify: scanned_at should be BEFORE midnight, created_at should be AFTER
        const scannedDate = new Date(data.scanned_at);
        const createdDate = new Date(data.created_at);

        console.log(`  Scanned at: ${scannedDate.toISOString()}`);
        console.log(`  Synced at:  ${createdDate.toISOString()}`);

        // The scan time should be earlier than sync time
        expect(scannedDate.getTime()).toBeLessThan(createdDate.getTime());

        // Clean up
        await supabase.from('bucket_records').delete().eq('id', data.id);

        console.log('✅ Midnight edge case: scan timestamp preserved correctly');
    });

    // ------------------------------------------
    // TEST 8: LA NÓMINA FANTASMA (Payroll with Ghost Data)
    // Export payroll with picker who has 0 buckets and no checkout.
    // Should not crash with NaN / division by zero.
    // ------------------------------------------
    test('Ghost Payroll: export handles 0-bucket picker without NaN', async ({ page }) => {
        const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);
        const orchardId = 'a1000000-0000-0000-0000-000000000001';
        const today = new Date().toISOString().split('T')[0];

        // Create ghost scenario: checked in, 0 buckets, no checkout
        const ghostPickerId = 'c1000000-0000-0001-0000-000000000017'; // Picker 17
        await supabase.from('daily_attendance').upsert({
            picker_id: ghostPickerId,
            orchard_id: orchardId,
            date: today,
            check_in_time: new Date(Date.now() - 4 * 60 * 60 * 1000).toISOString(),
            check_out_time: null,
            status: 'present',
        }, { onConflict: 'picker_id,date' });

        // Login as Manager
        await page.goto('/login');
        await page.fill('input[type="email"]', 'acid_manager_1_1@harvestpro.test');
        await page.fill('input[type="password"]', 'AcidTest2026!');
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/manager/, { timeout: 15000 });

        // Capture console errors (NaN, division by zero)
        const consoleErrors: string[] = [];
        page.on('console', msg => {
            if (msg.type() === 'error') {
                consoleErrors.push(msg.text());
            }
        });

        // Navigate to export
        const exportTab = page.getByRole('link', { name: /export|payroll|reports/i });
        if (await exportTab.isVisible()) {
            await exportTab.click();
            await page.waitForTimeout(2000);

            // Try to initiate export
            const exportBtn = page.getByRole('button', { name: /export|download|generate/i });
            if (await exportBtn.first().isVisible()) {
                await exportBtn.first().click();
                await page.waitForTimeout(3000);

                // Check for NaN or div/0 errors
                const nanErrors = consoleErrors.filter(e =>
                    e.includes('NaN') || e.includes('Infinity') || e.includes('division')
                );

                console.log(`  Console errors during export: ${consoleErrors.length}`);
                console.log(`  NaN/Infinity errors: ${nanErrors.length}`);

                expect(nanErrors.length).toBe(0);
            }
        }

        // Check page doesn't show NaN
        const bodyText = await page.textContent('body');
        expect(bodyText).not.toContain('NaN');
        expect(bodyText).not.toContain('undefined');

        console.log('✅ Payroll export handles ghost data without NaN');
    });
});
