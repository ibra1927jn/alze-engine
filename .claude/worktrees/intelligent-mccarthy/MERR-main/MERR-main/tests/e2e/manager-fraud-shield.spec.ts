/**
 * E2E Tests: Fraud Shield (Anomaly Detection)
 *
 * Tests for the AnomalyDetectionView inside the Insights tab,
 * including filter chips, anomaly cards, and smart dismissals.
 */
import { test, expect } from '@playwright/test';

const EMAIL = process.env.TEST_MANAGER_EMAIL || 'manager@harvestpro.nz';
const PASSWORD = process.env.TEST_MANAGER_PASSWORD || '111111';

async function navigateToFraudShield(page: import('@playwright/test').Page) {
    await page.goto('/');
    await page.fill('input[type="email"]', EMAIL);
    await page.fill('input[type="password"]', PASSWORD);
    await page.click('button[type="submit"]');
    await expect(page).toHaveURL(/\/manager/, { timeout: 15000 });

    // Navigate to Insights → Fraud Shield
    await page.getByText('Insights').click();
    await page.getByText(/Fraud|Shield|Anomal/i).first().click();
    // Wait for anomaly section to load
    await expect(page.locator('text=/Fraud Shield/i')).toBeVisible({ timeout: 10000 });
}

test.describe('Manager — Fraud Shield', () => {
    test.beforeEach(async ({ page }) => {
        await navigateToFraudShield(page);
    });

    test('Fraud Shield header shows severity counters', async ({ page }) => {
        // Should show High, Medium, Low, Dismissed counters
        await expect(page.locator('text=/High/i').first()).toBeVisible();
        await expect(page.locator('text=/Medium/i').first()).toBeVisible();
        await expect(page.locator('text=/Low/i').first()).toBeVisible();
        await expect(page.locator('text=/Dismissed/i').first()).toBeVisible();
    });

    test('Shows Demo or Live badge', async ({ page }) => {
        // Should show either Demo or Live badge depending on Edge Function status
        const badge = page.locator('text=/Demo|Live/i').first();
        await expect(badge).toBeVisible();
    });

    test('Shows smart rules explanation cards', async ({ page }) => {
        // Three rule explanation cards
        await expect(page.locator('text=/Elapsed Time/i').first()).toBeVisible();
        await expect(page.locator('text=/Peer Check/i').first()).toBeVisible();
        await expect(page.locator('text=/Grace Period/i').first()).toBeVisible();
    });

    test('Filter chips are accessible and functional', async ({ page }) => {
        // Should show filter chips
        const allFilter = page.locator('text=/All Flags/i').first();
        await expect(allFilter).toBeVisible();

        // Click a specific filter
        const peerFilter = page.locator('text=/Peer Outlier/i').first();
        if (await peerFilter.isVisible()) {
            await peerFilter.click();
            // Should filter the anomaly cards
            await page.waitForTimeout(500);
            // Click back to All
            await allFilter.click();
        }
    });

    test('Anomaly cards show picker information', async ({ page }) => {
        // Cards should contain picker names and severity badges
        const cards = page.locator('[class*="card"]');
        const cardCount = await cards.count();

        if (cardCount > 0) {
            // First card should have a risk badge
            const firstCard = cards.first();
            await expect(firstCard.locator('text=/risk/i')).toBeVisible();
        }
    });

    test('Smart Dismissals section is expandable', async ({ page }) => {
        // Find the Smart Dismissals section
        const dismissalBtn = page.locator('text=/Smart Dismissals/i').first();
        await expect(dismissalBtn).toBeVisible();

        // Click to expand
        await dismissalBtn.click();
        await page.waitForTimeout(500);

        // Should show dismissed scenarios
        await expect(
            page.locator('text=/Runner scanned|Entire Row|María/i').first()
        ).toBeVisible({ timeout: 5000 });
    });

    test('Refresh button exists and is clickable', async ({ page }) => {
        // Find the refresh button
        const refreshBtn = page.locator('button[title="Refresh anomalies"], button:has(span:text("refresh"))').first();
        if (await refreshBtn.isVisible()) {
            await refreshBtn.click();
            // Should not crash
            await page.waitForTimeout(1000);
            await expect(page.locator('text=/Fraud Shield/i')).toBeVisible();
        }
    });
});
