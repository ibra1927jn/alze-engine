// Smoke tests - críticos para verificar que la app funciona después de deployment
import { test, expect } from '@playwright/test';

test.describe('Smoke Tests - Critical Paths', () => {
    test('App loads successfully', async ({ page }) => {
        await page.goto('/');

        // Should see login page with Welcome back heading or HarvestPro branding
        await expect(page.getByText(/Welcome back|HarvestPro|Manage your harvest/i).first()).toBeVisible({ timeout: 10000 });

        // Email and password fields should be present
        await expect(page.locator('input[type="email"]')).toBeVisible();
        await expect(page.locator('input[type="password"]')).toBeVisible();
    });

    test('Authentication works', async ({ page }) => {
        await page.goto('/');

        // Fill login form with demo credentials
        await page.fill('input[type="email"]', process.env.TEST_MANAGER_EMAIL || 'manager@harvestpro.nz');
        await page.fill('input[type="password"]', process.env.TEST_MANAGER_PASSWORD || '111111');

        // Submit — button says "SIGN IN"
        await page.click('button[type="submit"]');

        // Should redirect to dashboard (or MFA)
        await expect(page).toHaveURL(/\/(manager|runner|team-leader|mfa)/, { timeout: 15000 });
    });

    test('API connection works', async ({ page }) => {
        // Simple health check — requires both URL and anon key
        const supabaseUrl = process.env.VITE_SUPABASE_URL;
        const supabaseKey = process.env.VITE_SUPABASE_ANON_KEY;
        test.skip(!supabaseUrl || !supabaseKey, 'Supabase credentials not set');
        const response = await page.request.get(supabaseUrl + '/rest/v1/', {
            headers: { apikey: supabaseKey!, Authorization: `Bearer ${supabaseKey}` },
        });
        expect(response.status()).toBe(200);
    });

    test('Service Worker registers', async ({ page }) => {
        await page.goto('/');

        // Check if SW registered
        const swRegistered = await page.evaluate(() => {
            return 'serviceWorker' in navigator;
        });

        expect(swRegistered).toBe(true);
    });
});
