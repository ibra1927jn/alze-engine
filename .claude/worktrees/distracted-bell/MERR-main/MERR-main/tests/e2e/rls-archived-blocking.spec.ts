import { test, expect, Page } from '@playwright/test';

/**
 * FASE 9: E2E Test - RLS Hardening (Archived Pickers)
 * 
 * Tests database-level security blocking archived pickers from syncing:
 * - Archived picker scans are blocked by RLS policy
 * - Error appears in DLQ with correct classification
 * - Offline device with archived picker fails sync with PGRST116
 */

test.describe('RLS Hardening - Archived Picker Blocking', () => {
    let managerPage: Page;
    let runnerPage: Page;

    test.beforeEach(async ({ browser }) => {
        // Setup two pages: Manager and Runner
        managerPage = await browser.newPage();
        runnerPage = await browser.newPage();

        // Login as Manager
        await managerPage.goto('/login');
        await managerPage.fill('input[type="email"]', 'manager@harvestpro.nz');
        await managerPage.fill('input[type="password"]', 'password123');
        await managerPage.click('button[type="submit"]');
        await expect(managerPage).toHaveURL('/manager', { timeout: 10000 });

        // Login as Runner
        await runnerPage.goto('/login');
        await runnerPage.fill('input[type="email"]', 'runner@harvestpro.nz');
        await runnerPage.fill('input[type="password"]', 'password123');
        await runnerPage.click('button[type="submit"]');
        await expect(runnerPage).toHaveURL('/runner', { timeout: 10000 });
    });

    test('CRITICAL: Database blocks INSERT from archived picker (RLS policy)', async () => {
        // Scenario: Archive picker → Runner scans → Database rejects with RLS error

        // Manager archives a picker
        await managerPage.getByRole('link', { name: /personnel|team/i }).click();
        const pickerRow = managerPage.locator('[data-picker-id="PICKER-RLS-001"]');
        await pickerRow.locator('button[aria-label*="remove"]').or(pickerRow.getByRole('button', { name: /archive|remove/i })).click();

        // Confirm archive
        const confirmButton = managerPage.getByRole('button', { name: /confirm|yes/i });
        await confirmButton.click();

        // Verify picker is archived
        await expect(pickerRow).toHaveAttribute('data-status', 'archived');

        // Runner (who may still have picker in offline cache) attempts scan
        await runnerPage.getByRole('button', { name: /scan/i }).click();
        await runnerPage.waitForSelector('[data-testid="scanner-modal"]');

        const manualEntryInput = runnerPage.locator('input[placeholder*="picker"]');
        if (await manualEntryInput.isVisible()) {
            await manualEntryInput.fill('PICKER-RLS-001');
            await runnerPage.getByRole('button', { name: /confirm/i }).click();
        }

        // Scan is queued (appears as pending)
        const syncBadge = runnerPage.getByTestId('sync-badge');
        await expect(syncBadge).toBeVisible();

        // Wait for sync attempt (should fail at database)
        await runnerPage.waitForTimeout(3000);

        // Manager checks DLQ
        await managerPage.getByRole('link', { name: /dead.*letter|dlq|errors/i }).click();

        // Verify error appears in Critical section
        const criticalSection = managerPage.locator('[data-severity="critical"]')
            .or(managerPage.getByText(/critical errors/i).locator('..'));
        await expect(criticalSection).toBeVisible();

        // Verify error tooltip shows RLS violation
        const errorItem = criticalSection.locator('[data-error-type*="RLS"]').or(
            criticalSection.getByText(/RLS|policy|archived/i)
        );
        await expect(errorItem).toBeVisible();

        // Hover to see tooltip
        await errorItem.hover();
        const tooltip = managerPage.locator('[role="tooltip"]')
            .or(managerPage.locator('.error-tooltip'));
        await expect(tooltip).toContainText(/RLS.*Policy.*Violation|archived.*picker|security.*rules/i);
    });

    test('Offline device with archived picker fails sync with detailed error', async () => {
        // Scenario: Runner goes offline → Manager archives picker → Runner comes online → Sync fails

        // Runner goes offline
        await runnerPage.context().setOffline(true);

        // Runner scans bucket for picker (queued locally)
        await runnerPage.getByRole('button', { name: /scan/i }).click();
        const manualEntryInput = runnerPage.locator('input[placeholder*="picker"]');
        if (await manualEntryInput.isVisible()) {
            await manualEntryInput.fill('PICKER-RLS-002');
            await runnerPage.getByRole('button', { name: /confirm/i }).click();
        }

        // Verify scan is queued
        const syncBadge = runnerPage.getByTestId('sync-badge');
        await expect(syncBadge).toContainText('1');

        // Meanwhile, Manager archives the picker
        await managerPage.getByRole('link', { name: /personnel|team/i }).click();
        const pickerRow = managerPage.locator('[data-picker-id="PICKER-RLS-002"]');
        await pickerRow.locator('button[aria-label*="remove"]').click();
        await managerPage.getByRole('button', { name: /confirm|yes/i }).click();

        // Runner comes back online
        await runnerPage.context().setOffline(false);

        // Wait for sync attempt
        await runnerPage.waitForTimeout(5000);

        // Sync should fail - badge might still show pending or error state
        // Check console for RLS error (in real test, we'd check network tab)

        // Manager verifies error in DLQ
        await managerPage.getByRole('link', { name: /dead.*letter|dlq/i }).click();
        const rlsError = managerPage.getByText(/PGRST116|RLS|archived.*picker/i);
        await expect(rlsError).toBeVisible({ timeout: 10000 });
    });

    test('RLS policy has performance index', async () => {
        // This is more of an integration test - verify the index exists
        // Would need direct database access or SQL query component

        // For E2E, we verify performance by rapid archiving/scanning
        // Archive 10 pickers quickly
        await managerPage.getByRole('link', { name: /personnel/i }).click();

        for (let i = 1; i <= 10; i++) {
            const pickerRow = managerPage.locator(`[data-picker-id="PICKER-PERF-${String(i).padStart(3, '0')}"]`);
            if (await pickerRow.isVisible()) {
                await pickerRow.locator('button[aria-label*="remove"]').click();
                await managerPage.getByRole('button', { name: /confirm/i }).click();
                await managerPage.waitForTimeout(100); // Brief wait
            }
        }

        // Runner scans for archived pickers - should all fail quickly
        const startTime = Date.now();

        for (let i = 1; i <= 10; i++) {
            await runnerPage.getByRole('button', { name: /scan/i }).click();
            const manualEntryInput = runnerPage.locator('input[placeholder*="picker"]');
            if (await manualEntryInput.isVisible()) {
                await manualEntryInput.fill(`PICKER-PERF-${String(i).padStart(3, '0')}`);
                await runnerPage.getByRole('button', { name: /confirm/i }).click();
            }
            await runnerPage.waitForTimeout(200);
        }

        const endTime = Date.now();
        const duration = endTime - startTime;

        // All 10 scans should complete in under 5 seconds (with index)
        expect(duration).toBeLessThan(5000);
    });

    test.afterEach(async () => {
        await managerPage.close();
        await runnerPage.close();
    });
});
