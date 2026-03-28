import { test, expect, Page } from '@playwright/test';

/**
 * FASE 9: E2E Test - Payroll Calculation Accuracy
 * 
 * Tests that payroll calculations match the Edge Function formula:
 * - Piece rate earnings calculated correctly
 * - Minimum wage supplements applied when needed
 * - Archived pickers excluded from calculations
 * - Dashboard updates in real-time
 */

test.describe('Payroll Calculation Accuracy', () => {
    let managerPage: Page;
    let runnerPage: Page;

    test.beforeEach(async ({ browser }) => {
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

        // Configure settings: piece_rate = $5, min_wage = $23.15
        await managerPage.getByRole('link', { name: /settings/i }).click();
        await managerPage.fill('input[name="piece_rate"]', '5.00');
        await managerPage.fill('input[name="min_wage_rate"]', '23.15');
        await managerPage.getByRole('button', { name: /save/i }).click();
    });

    test('CRITICAL: Picker above minimum wage (no supplement)', async () => {
        // Test Data: 20 buckets × $5 = $100 piece rate
        // 4 hours × $23.15 = $92.60 minimum wage threshold
        // Expected: $100 total (no supplement needed)

        // Runner scans 20 buckets for PICKER-PAYROLL-HIGH
        for (let i = 1; i <= 20; i++) {
            await runnerPage.getByRole('button', { name: /scan/i }).click();
            const input = runnerPage.locator('input[placeholder*="picker"]');
            await input.fill('PICKER-PAYROLL-HIGH');
            await runnerPage.getByRole('button', { name: /confirm/i }).click();
            await runnerPage.waitForTimeout(200);
        }

        // Check Manager dashboard
        await managerPage.getByRole('link', { name: /dashboard/i }).click();

        // Verify payroll calculation
        const earningsSection = managerPage.locator('[data-testid="earnings"]')
            .or(managerPage.getByText(/total.*cost|earnings|payroll/i).locator('..'));

        await expect(earningsSection).toContainText(/\$100\.00|\$100/);

        // Verify breakdown shows zero supplement
        const supplementField = managerPage.locator('[data-testid="minimum-supplement"]');
        if (await supplementField.isVisible()) {
            await expect(supplementField).toContainText(/\$0\.00|\$0/);
        }
    });

    test('CRITICAL: Picker below minimum wage (supplement applied)', async () => {
        // Test Data: 10 buckets × $5 = $50 piece rate
        // 4 hours × $23.15 = $92.60 minimum wage threshold
        // Expected: $50 piece + $42.60 supplement = $92.60 total

        // Runner scans 10 buckets for PICKER-PAYROLL-LOW
        for (let i = 1; i <= 10; i++) {
            await runnerPage.getByRole('button', { name: /scan/i }).click();
            const input = runnerPage.locator('input[placeholder*="picker"]');
            await input.fill('PICKER-PAYROLL-LOW');
            await runnerPage.getByRole('button', { name: /confirm/i }).click();
            await runnerPage.waitForTimeout(200);
        }

        // Check Manager dashboard
        await managerPage.getByRole('link', { name: /dashboard/i }).click();

        // Verify total payroll includes supplement
        const earningsSection = managerPage.locator('[data-testid="earnings"]');
        await expect(earningsSection).toContainText(/\$92\.60|\$92\.6/);

        // Verify breakdown shows supplement
        const pieceField = managerPage.locator('[data-testid="piece-earnings"]');
        if (await pieceField.isVisible()) {
            await expect(pieceField).toContainText(/\$50/);
        }

        const supplementField = managerPage.locator('[data-testid="minimum-supplement"]');
        if (await supplementField.isVisible()) {
            await expect(supplementField).toContainText(/\$42\.60|\$42\.6/);
        }
    });

    test('CRITICAL: Archived picker excluded from payroll', async () => {
        // Scan 15 buckets for PICKER-PAYROLL-ARCHIVE
        for (let i = 1; i <= 15; i++) {
            await runnerPage.getByRole('button', { name: /scan/i }).click();
            const input = runnerPage.locator('input[placeholder*="picker"]');
            await input.fill('PICKER-PAYROLL-ARCHIVE');
            await runnerPage.getByRole('button', { name: /confirm/i }).click();
            await runnerPage.waitForTimeout(100);
        }

        // Get initial payroll total
        await managerPage.getByRole('link', { name: /dashboard/i }).click();
        const initialEarnings = await managerPage.locator('[data-testid="total-earnings"]').textContent();
        const initialAmount = parseFloat(initialEarnings?.replace(/[^0-9.]/g, '') || '0');

        // Archive the picker
        await managerPage.getByRole('link', { name: /personnel/i }).click();
        const pickerRow = managerPage.locator('[data-picker-id="PICKER-PAYROLL-ARCHIVE"]');
        await pickerRow.locator('button[aria-label*="remove"]').click();
        await managerPage.getByRole('button', { name: /confirm/i }).click();

        // Wait for payroll recalculation
        await managerPage.waitForTimeout(2000);

        // Check dashboard again
        await managerPage.getByRole('link', { name: /dashboard/i }).click();
        const newEarnings = await managerPage.locator('[data-testid="total-earnings"]').textContent();
        const newAmount = parseFloat(newEarnings?.replace(/[^0-9.]/g, '') || '0');

        // Payroll should decrease by $75 (15 buckets × $5)
        expect(newAmount).toBeLessThan(initialAmount);
        expect(initialAmount - newAmount).toBeCloseTo(75, 2);
    });

    test('Real-time payroll updates on every scan', async () => {
        // Monitor dashboard for real-time updates
        await managerPage.getByRole('link', { name: /dashboard/i }).click();

        // Get initial payroll
        const initialText = await managerPage.locator('[data-testid="total-earnings"]').textContent();
        const initialAmount = parseFloat(initialText?.replace(/[^0-9.]/g, '') || '0');

        // Runner scans 1 bucket
        await runnerPage.getByRole('button', { name: /scan/i }).click();
        const input = runnerPage.locator('input[placeholder*="picker"]');
        await input.fill('PICKER-PAYROLL-REALTIME');
        await runnerPage.getByRole('button', { name: /confirm/i }).click();

        // Wait for real-time update (should be < 1 second)
        await managerPage.waitForTimeout(1500);

        // Verify payroll increased
        const newText = await managerPage.locator('[data-testid="total-earnings"]').textContent();
        const newAmount = parseFloat(newText?.replace(/[^0-9.]/g, '') || '0');

        expect(newAmount).toBeGreaterThan(initialAmount);
        expect(newAmount - initialAmount).toBeCloseTo(5, 2); // $5 per bucket
    });

    test.afterEach(async () => {
        await managerPage.close();
        await runnerPage.close();
    });
});
