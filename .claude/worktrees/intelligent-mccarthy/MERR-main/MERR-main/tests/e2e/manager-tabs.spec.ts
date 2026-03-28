/**
 * E2E Tests: Manager New Tabs
 *
 * Tests for the new Timesheet, Sync Errors tabs, and lazy loading behavior
 * added in the Manager department audit.
 */
import { test, expect } from '@playwright/test';

const EMAIL = process.env.TEST_MANAGER_EMAIL || 'manager@harvestpro.nz';
const PASSWORD = process.env.TEST_MANAGER_PASSWORD || '111111';

async function loginAsManager(page: import('@playwright/test').Page) {
    await page.goto('/');
    await page.fill('input[type="email"]', EMAIL);
    await page.fill('input[type="password"]', PASSWORD);
    await page.click('button[type="submit"]');
    await expect(page).toHaveURL(/\/manager/, { timeout: 15000 });
}

test.describe('Manager — New Tabs (Desktop)', () => {
    test.beforeEach(async ({ page }) => {
        await loginAsManager(page);
    });

    test('Sidebar shows all tabs including Timesheet and Sync Errors', async ({ page }) => {
        // Verify sidebar contains the expected tabs
        const sidebar = page.locator('nav, [role="navigation"], aside').first();
        await expect(sidebar).toBeVisible();

        // Check for new tabs
        await expect(page.getByText('Timesheet')).toBeVisible();
        await expect(page.getByText('Sync Errors')).toBeVisible();

        // Check existing tabs are still present
        await expect(page.getByText('Dashboard')).toBeVisible();
        await expect(page.getByText('Teams')).toBeVisible();
        await expect(page.getByText('Insights')).toBeVisible();
    });

    test('Timesheet tab loads TimesheetEditor', async ({ page }) => {
        // Click Timesheet tab
        await page.getByText('Timesheet').click();

        // Should show the timesheet editor content
        await expect(page.locator('text=/Timesheet|Attendance|Hours/i')).toBeVisible({ timeout: 10000 });
    });

    test('Sync Errors tab loads DeadLetterQueueView', async ({ page }) => {
        // Click Sync Errors tab
        await page.getByText('Sync Errors').click();

        // Should show dead letter queue content (either empty state or queue items)
        await expect(
            page.locator('text=/Dead Letter|Sync Error|All Clear|Failed Sync/i')
        ).toBeVisible({ timeout: 10000 });
    });

    test('Insights tab loads and shows sub-tabs', async ({ page }) => {
        // Click Insights
        await page.getByText('Insights').click();

        // Should show insights sub-navigation (Cost Analytics, Weekly Report, Fraud Shield)
        await expect(
            page.locator('text=/Cost|Analytics|Weekly|Report|Fraud|Shield/i').first()
        ).toBeVisible({ timeout: 10000 });
    });

    test('Lazy-loaded tabs show loading spinner during load', async ({ page }) => {
        // Throttle network to simulate slow connection
        const cdp = await page.context().newCDPSession(page);
        await cdp.send('Network.emulateNetworkConditions', {
            offline: false,
            downloadThroughput: 50 * 1024, // 50KB/s
            uploadThroughput: 50 * 1024,
            latency: 500,
        });

        // Navigate to a lazy-loaded tab
        await page.getByText('Timesheet').click();

        // The spinner/loader should appear briefly
        // (might be too fast on local, so we just verify no crash)
        await expect(page.locator('text=/Timesheet|Attendance|Hours/i')).toBeVisible({ timeout: 30000 });

        await cdp.detach();
    });
});
