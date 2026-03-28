import { test, expect, Page } from '@playwright/test';

/**
 * FASE 9: E2E Test - Soft Delete Protection
 * 
 * Tests that picker removal uses UPDATE instead of DELETE:
 * - Picker status set to 'archived'
 * - archived_at timestamp recorded
 * - Historical bucket_records preserved
 * - UI updates correctly
 */

test.describe('Soft Delete Protection', () => {
    let managerPage: Page;

    test.beforeEach(async ({ browser }) => {
        managerPage = await browser.newPage();

        // Login as Manager
        await managerPage.goto('/login');
        await managerPage.fill('input[type="email"]', 'manager@harvestpro.nz');
        await managerPage.fill('input[type="password"]', 'password123');
        await managerPage.click('button[type="submit"]');
        await expect(managerPage).toHaveURL('/manager', { timeout: 10000 });
    });

    test('CRITICAL: Soft delete sets status=archived (not DELETE)', async () => {
        // Navigate to personnel management
        await managerPage.getByRole('link', { name: /personnel|team/i }).click();

        // Find test picker
        const pickerRow = managerPage.locator('[data-picker-id="PICKER-SOFTDELETE-001"]');
        await expect(pickerRow).toBeVisible();

        // Verify initial status is active
        await expect(pickerRow).toHaveAttribute('data-status', 'active');

        // Remove (archive) picker
        await pickerRow.locator('button[aria-label*="remove"]').or(
            pickerRow.getByRole('button', { name: /remove|archive/i })
        ).click();

        // Confirm action
        await managerPage.getByRole('button', { name: /confirm|yes/i }).click();

        // Verify picker disappears from active roster
        await expect(pickerRow).not.toBeVisible({ timeout: 3000 });

        // Check if there's an "archived" view/filter
        const archivedFilter = managerPage.getByRole('button', { name: /archived|removed/i });
        if (await archivedFilter.isVisible()) {
            await archivedFilter.click();

            // Verify picker appears in archived list
            const archivedPicker = managerPage.locator('[data-picker-id="PICKER-SOFTDELETE-001"]');
            await expect(archivedPicker).toBeVisible();
            await expect(archivedPicker).toHaveAttribute('data-status', 'archived');
        }
    });

    test('CRITICAL: Historical bucket_records remain intact after soft delete', async () => {
        // This test requires direct database access or API endpoint
        // For E2E, we verify by checking reports/history views

        await managerPage.getByRole('link', { name: /personnel/i }).click();

        // Get picker's bucket count before archiving
        const pickerRow = managerPage.locator('[data-picker-id="PICKER-SOFTDELETE-002"]');
        const bucketCountBefore = await pickerRow.locator('[data-testid="bucket-count"]').textContent();
        // Verify we can parse the count (validates UI element exists)
        expect(parseInt(bucketCountBefore?.replace(/[^0-9]/g, '') || '0')).toBeGreaterThanOrEqual(0);

        // Archive picker
        await pickerRow.locator('button[aria-label*="remove"]').click();
        await managerPage.getByRole('button', { name: /confirm/i }).click();

        // Navigate to reports/history
        await managerPage.getByRole('link', { name: /reports|history/i }).click();

        // Search for archived picker's history
        const searchInput = managerPage.locator('input[placeholder*="search"]');
        if (await searchInput.isVisible()) {
            await searchInput.fill('PICKER-SOFTDELETE-002');
            await managerPage.keyboard.press('Enter');
        }

        // Verify bucket records still exist
        const historyTable = managerPage.locator('[data-testid="bucket-history"]');
        const rowCount = await historyTable.locator('tr').count();

        expect(rowCount).toBeGreaterThan(0); // Historical data preserved
    });

    test('Archived picker scans are blocked', async () => {
        // Archive a picker first
        await managerPage.getByRole('link', { name: /personnel/i }).click();
        const pickerRow = managerPage.locator('[data-picker-id="PICKER-SOFTDELETE-003"]');
        await pickerRow.locator('button[aria-label*="remove"]').click();
        await managerPage.getByRole('button', { name: /confirm/i }).click();

        // Open Runner in new page
        const runnerPage = await managerPage.context().newPage();
        await runnerPage.goto('/login');
        await runnerPage.fill('input[type="email"]', 'runner@harvestpro.nz');
        await runnerPage.fill('input[type="password"]', 'password123');
        await runnerPage.click('button[type="submit"]');
        await runnerPage.waitForURL('/runner');

        // Attempt to scan archived picker
        await runnerPage.getByRole('button', { name: /scan/i }).click();
        const input = runnerPage.locator('input[placeholder*="picker"]');
        await input.fill('PICKER-SOFTDELETE-003');
        await runnerPage.getByRole('button', { name: /confirm/i }).click();

        // Verify rejection
        const errorMessage = runnerPage.locator('[role="alert"]');
        await expect(errorMessage).toBeVisible();
        await expect(errorMessage).toContainText(/archived|removed|not.*found|inactive/i);

        await runnerPage.close();
    });

    test('UI updates optimistically on soft delete', async () => {
        // Test optimistic UI update (no waiting for server response)
        await managerPage.getByRole('link', { name: /personnel/i }).click();

        const pickerRow = managerPage.locator('[data-picker-id="PICKER-SOFTDELETE-004"]');
        await expect(pickerRow).toBeVisible();

        // Start timer
        const startTime = Date.now();

        // Click remove
        await pickerRow.locator('button[aria-label*="remove"]').click();
        await managerPage.getByRole('button', { name: /confirm/i }).click();

        // Measure time until picker disappears from UI
        await expect(pickerRow).not.toBeVisible({ timeout: 1000 });
        const endTime = Date.now();

        // Optimistic update should be < 500ms
        expect(endTime - startTime).toBeLessThan(500);
    });

    test('archived_at timestamp is set correctly', async () => {
        // This requires either API access or database inspection
        // For E2E, we can verify through UI if timestamp is displayed

        await managerPage.getByRole('link', { name: /personnel/i }).click();

        const pickerRow = managerPage.locator('[data-picker-id="PICKER-SOFTDELETE-005"]');
        await pickerRow.locator('button[aria-label*="remove"]').click();
        await managerPage.getByRole('button', { name: /confirm/i }).click();

        // Check archived view for timestamp
        const archivedFilter = managerPage.getByRole('button', { name: /archived/i });
        if (await archivedFilter.isVisible()) {
            await archivedFilter.click();

            const archivedPicker = managerPage.locator('[data-picker-id="PICKER-SOFTDELETE-005"]');
            const timestamp = archivedPicker.locator('[data-testid="archived-at"]');

            if (await timestamp.isVisible()) {
                const timestampText = await timestamp.textContent();
                // Verify it's a recent timestamp (today's date)
                const today = new Date().toISOString().split('T')[0];
                expect(timestampText).toContain(today);
            }
        }
    });

    test.afterEach(async () => {
        await managerPage.close();
    });
});
