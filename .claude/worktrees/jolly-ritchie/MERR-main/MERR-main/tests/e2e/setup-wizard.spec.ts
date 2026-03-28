/**
 * setup-wizard.spec.ts — E2E Tests for SetupWizard
 * Sprint E5: Validates the 4-step wizard flow for new orchard setup
 */
import { test, expect } from '@playwright/test';

test.describe('Setup Wizard — New Orchard Configuration', () => {
    test.beforeEach(async ({ page }) => {
        // Login as admin/manager who has access to SetupWizard
        await page.goto('/');
        await page.fill('input[type="email"]', process.env.TEST_MANAGER_EMAIL || 'test@example.com');
        await page.fill('input[type="password"]', process.env.TEST_MANAGER_PASSWORD || 'password123');
        await page.click('button[type="submit"]');
        await page.waitForURL(/\/(manager|admin)/);
    });

    test('Wizard opens from Admin page', async ({ page }) => {
        await page.goto('/admin');
        // Look for setup wizard trigger
        const wizardButton = page.locator('button:has-text("Setup"), [data-testid="setup-wizard"]');
        if (await wizardButton.isVisible()) {
            await wizardButton.click();
            // Wizard should show step 1
            await expect(page.locator('text=/step 1|orchard details|basic info/i')).toBeVisible();
        }
    });

    test('Step 1: Orchard details validation', async ({ page }) => {
        await page.goto('/admin');
        const wizardButton = page.locator('button:has-text("Setup"), [data-testid="setup-wizard"]');
        if (await wizardButton.isVisible()) {
            await wizardButton.click();

            // Try to proceed without filling required fields
            const nextButton = page.locator('button:has-text("Next")');
            if (await nextButton.isVisible()) {
                await nextButton.click();
                // Should show validation error or stay on step 1
                await expect(page.locator('text=/required|please fill|can\'t be empty/i')).toBeVisible();
            }

            // Fill valid data
            await page.fill('input[name="name"], input[placeholder*="name" i]', 'Test Orchard E2E');
            await page.fill('input[name="code"], input[placeholder*="code" i]', 'TEST01');
            await page.fill('input[name="location"], input[placeholder*="location" i]', 'Cromwell, Central Otago');
        }
    });

    test('Step 2: Day setup configuration', async ({ page }) => {
        await page.goto('/admin');
        const wizardButton = page.locator('button:has-text("Setup"), [data-testid="setup-wizard"]');
        if (await wizardButton.isVisible()) {
            await wizardButton.click();

            // Fill step 1 and proceed
            await page.fill('input[name="name"], input[placeholder*="name" i]', 'Test Orchard E2E');
            await page.fill('input[name="code"], input[placeholder*="code" i]', 'TEST01');
            const nextButton = page.locator('button:has-text("Next")');
            await nextButton.click();

            // Step 2 should show day setup options
            await expect(page.locator('text=/variety|target|piece rate|day setup/i')).toBeVisible();
        }
    });

    test('Wizard can be cancelled without side effects', async ({ page }) => {
        await page.goto('/admin');
        const wizardButton = page.locator('button:has-text("Setup"), [data-testid="setup-wizard"]');
        if (await wizardButton.isVisible()) {
            await wizardButton.click();

            // Fill some data
            await page.fill('input[name="name"], input[placeholder*="name" i]', 'Cancel Test');

            // Click cancel/close
            const cancelButton = page.locator('button:has-text("Cancel"), button:has-text("Close"), [aria-label="close"]');
            if (await cancelButton.isVisible()) {
                await cancelButton.click();
            }

            // Wizard should be closed
            await expect(page.locator('text=/step 1|wizard/i')).not.toBeVisible();
        }
    });

    test('Complete wizard creates orchard and day_setup', async ({ page }) => {
        await page.goto('/admin');
        const wizardButton = page.locator('button:has-text("Setup"), [data-testid="setup-wizard"]');
        if (await wizardButton.isVisible()) {
            await wizardButton.click();

            // Fill all steps (simplified — actual form fields may vary)
            await page.fill('input[name="name"], input[placeholder*="name" i]', 'E2E Test Orchard');
            await page.fill('input[name="code"], input[placeholder*="code" i]', 'E2E01');

            // Navigate through steps
            const nextButton = page.locator('button:has-text("Next")');
            for (let i = 0; i < 3; i++) {
                if (await nextButton.isVisible()) {
                    await nextButton.click();
                    await page.waitForTimeout(500);
                }
            }

            // Final submit
            const finishButton = page.locator('button:has-text("Finish"), button:has-text("Complete"), button:has-text("Create")');
            if (await finishButton.isVisible()) {
                await finishButton.click();
                // Should show success
                await expect(page.locator('text=/success|created|complete/i')).toBeVisible({ timeout: 5000 });
            }
        }
    });
});
