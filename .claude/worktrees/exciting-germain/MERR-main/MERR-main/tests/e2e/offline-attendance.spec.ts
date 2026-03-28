import { test, expect, Page } from '@playwright/test';

/**
 * FASE 9: E2E Test - Offline-First Attendance Guard
 * 
 * Tests the critical offline attendance validation functionality:
 * - Pickers not checked in cannot scan buckets (even offline)
 * - Checked-in pickers can scan successfully
 * - Real-time subscription updates cache instantly
 */

test.describe('Offline-First Attendance Guard', () => {
    let page: Page;

    test.beforeEach(async ({ browser }) => {
        page = await browser.newPage();

        // Login as Runner
        await page.goto('/login');
        await page.fill('input[type="email"]', 'runner@harvestpro.nz');
        await page.fill('input[type="password"]', 'password123');
        await page.click('button[type="submit"]');

        // Wait for Runner Dashboard
        await expect(page).toHaveURL('/runner', { timeout: 10000 });
    });

    test('CRITICAL: Reject scan for unchecked picker (online)', async () => {
        // Scenario: Picker NOT checked in → Scan bucket → REJECTED

        // Navigate to scanner
        await page.getByRole('button', { name: /scan/i }).click();

        // Simulate scanner modal (adjust selectors based on actual UI)
        await page.waitForSelector('[data-testid="scanner-modal"]');

        // Enter picker code (assuming manual entry for testing)
        const manualEntryInput = page.locator('input[placeholder*="picker"]');
        if (await manualEntryInput.isVisible()) {
            await manualEntryInput.fill('PICKER-UNCHECKED-001');
            await page.getByRole('button', { name: /confirm/i }).click();
        }

        // Verify rejection message
        const errorMessage = page.locator('[role="alert"]').or(page.getByText(/not checked in/i));
        await expect(errorMessage).toBeVisible({ timeout: 2000 });
        await expect(errorMessage).toContainText(/not checked in|attendance/i);
    });

    test('CRITICAL: Accept scan for checked-in picker (online)', async () => {
        // Scenario: Picker checked in → Scan bucket → SUCCESS

        // Navigate to scanner
        await page.getByRole('button', { name: /scan/i }).click();
        await page.waitForSelector('[data-testid="scanner-modal"]');

        // Enter checked-in picker code
        const manualEntryInput = page.locator('input[placeholder*="picker"]');
        if (await manualEntryInput.isVisible()) {
            await manualEntryInput.fill('PICKER-CHECKED-001');
            await page.getByRole('button', { name: /confirm/i }).click();
        }

        // Verify success
        const successIndicator = page.locator('[data-testid="scan-success"]')
            .or(page.getByText(/success|scanned/i));
        await expect(successIndicator).toBeVisible({ timeout: 2000 });
    });

    test('CRITICAL: Reject scan offline using attendance cache', async () => {
        // Scenario: Go offline → Scan unchecked picker → REJECTED (no network call)

        // Go offline BEFORE scanning
        await page.context().setOffline(true);

        // Verify offline indicator
        const offlineIndicator = page.locator('[data-testid="offline-indicator"]');
        await expect(offlineIndicator).toBeVisible({ timeout: 3000 });

        // Attempt to scan unchecked picker
        await page.getByRole('button', { name: /scan/i }).click();
        await page.waitForSelector('[data-testid="scanner-modal"]');

        const manualEntryInput = page.locator('input[placeholder*="picker"]');
        if (await manualEntryInput.isVisible()) {
            await manualEntryInput.fill('PICKER-UNCHECKED-002');
            await page.getByRole('button', { name: /confirm/i }).click();
        }

        // Verify rejection (using locally cached attendance data)
        const errorMessage = page.locator('[role="alert"]');
        await expect(errorMessage).toBeVisible({ timeout: 500 }); // Should be instant
        await expect(errorMessage).toContainText(/not checked in/i);

        // Restore online
        await page.context().setOffline(false);
    });

    test('Real-time attendance updates cache instantly', async () => {
        // Scenario: Picker check-in event → Cache updates → Immediately scannable

        // Listen for network traffic to verify real-time subscription
        const wsMessages: string[] = [];
        page.on('websocket', ws => {
            ws.on('framereceived', event => {
                wsMessages.push(event.payload.toString());
            });
        });

        // Open a second page as Team Leader to check in picker
        const teamLeaderPage = await page.context().newPage();
        await teamLeaderPage.goto('/login');
        await teamLeaderPage.fill('input[type="email"]', 'teamleader@harvestpro.nz');
        await teamLeaderPage.fill('input[type="password"]', 'password123');
        await teamLeaderPage.click('button[type="submit"]');
        await teamLeaderPage.waitForURL('/team-leader');

        // Check in a picker
        await teamLeaderPage.getByRole('button', { name: /check.*in/i }).click();
        await teamLeaderPage.locator('[data-picker-id="PICKER-REALTIME-001"]').click();

        // Wait for real-time event to propagate
        await page.waitForTimeout(1500); // Max 1.5 second latency

        // Verify runner can now scan this picker
        await page.getByRole('button', { name: /scan/i }).click();
        const manualEntryInput = page.locator('input[placeholder*="picker"]');
        if (await manualEntryInput.isVisible()) {
            await manualEntryInput.fill('PICKER-REALTIME-001');
            await page.getByRole('button', { name: /confirm/i }).click();
        }

        // Success
        const successIndicator = page.locator('[data-testid="scan-success"]');
        await expect(successIndicator).toBeVisible({ timeout: 2000 });

        await teamLeaderPage.close();
    });

    test('Attendance cache persists across page reload', async () => {
        // Verify cache survives reload (stored in Zustand persist or localStorage)

        // Reload page while offline
        await page.context().setOffline(true);
        await page.reload();

        // Verify offline mode still works
        await page.waitForSelector('[data-testid="offline-indicator"]');

        // Attempt scan with unchecked picker
        await page.getByRole('button', { name: /scan/i }).click();
        const manualEntryInput = page.locator('input[placeholder*="picker"]');
        if (await manualEntryInput.isVisible()) {
            await manualEntryInput.fill('PICKER-UNCHECKED-003');
            await page.getByRole('button', { name: /confirm/i }).click();
        }

        // Should still reject using persisted cache
        const errorMessage = page.locator('[role="alert"]');
        await expect(errorMessage).toBeVisible();
        await expect(errorMessage).toContainText(/not checked in/i);

        await page.context().setOffline(false);
    });

    test.afterEach(async () => {
        await page.close();
    });
});
