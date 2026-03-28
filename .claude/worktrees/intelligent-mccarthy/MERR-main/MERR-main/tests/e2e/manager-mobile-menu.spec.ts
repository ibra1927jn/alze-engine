/**
 * E2E Tests: Manager Mobile Menu
 *
 * Tests that the new Timesheet and Sync Errors items appear in
 * the mobile "More" menu and navigate correctly on small viewports.
 */
import { test, expect } from '@playwright/test';

const EMAIL = process.env.TEST_MANAGER_EMAIL || 'manager@harvestpro.nz';
const PASSWORD = process.env.TEST_MANAGER_PASSWORD || '111111';

test.describe('Manager — Mobile Menu', () => {
    test.use({ viewport: { width: 390, height: 844 } }); // iPhone 13 viewport

    test.beforeEach(async ({ page }) => {
        await page.goto('/');
        await page.fill('input[type="email"]', EMAIL);
        await page.fill('input[type="password"]', PASSWORD);
        await page.click('button[type="submit"]');
        await expect(page).toHaveURL(/\/manager/, { timeout: 15000 });
    });

    test('Bottom navigation shows More button on mobile', async ({ page }) => {
        // On mobile, should show bottom nav with a "More" button
        const moreBtn = page.locator('text=/More|Más/i').last();
        await expect(moreBtn).toBeVisible({ timeout: 5000 });
    });

    test('More menu contains Timesheet and Sync Errors', async ({ page }) => {
        // Tap More to open the menu
        const moreBtn = page.locator('text=/More|Más/i').last();
        await moreBtn.click();
        await page.waitForTimeout(500);

        // Should show Timesheet and Sync Errors in the menu
        await expect(page.locator('text=/Timesheet/i').first()).toBeVisible({ timeout: 5000 });
        await expect(page.locator('text=/Sync Errors/i').first()).toBeVisible({ timeout: 5000 });
    });

    test('Tapping Timesheet in More menu navigates correctly', async ({ page }) => {
        // Open More menu
        const moreBtn = page.locator('text=/More|Más/i').last();
        await moreBtn.click();
        await page.waitForTimeout(500);

        // Tap Timesheet
        await page.locator('text=/Timesheet/i').first().click();
        await page.waitForTimeout(1000);

        // Should show timesheet content
        await expect(
            page.locator('text=/Timesheet|Attendance|Hours/i').first()
        ).toBeVisible({ timeout: 10000 });
    });

    test('Tapping Sync Errors in More menu navigates correctly', async ({ page }) => {
        // Open More menu
        const moreBtn = page.locator('text=/More|Más/i').last();
        await moreBtn.click();
        await page.waitForTimeout(500);

        // Tap Sync Errors
        await page.locator('text=/Sync Errors/i').first().click();
        await page.waitForTimeout(1000);

        // Should show dead letter queue content
        await expect(
            page.locator('text=/Dead Letter|Sync Error|All Clear|Failed/i').first()
        ).toBeVisible({ timeout: 10000 });
    });
});
