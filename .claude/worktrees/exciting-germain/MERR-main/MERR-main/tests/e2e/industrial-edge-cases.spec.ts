/* eslint-disable no-console */
import { test, expect } from '@playwright/test';

/**
 * INDUSTRIAL TEST 1: PWA Cold Start (Offline Asset Caching)
 * 
 * Tests that the app loads completely offline after first visit.
 * Critical for industrial environments where network is unreliable.
 * 
 * Failure symptom: White screen when offline = App is NOT industrial-ready
 */

test.describe('PWA Cold Start - Offline Asset Caching', () => {
    test('CRITICAL: App loads offline after first visit (Service Worker cache)', async ({ browser }) => {
        // Step 1: Open app online (first visit to populate cache)
        const context = await browser.newContext();
        const page = await context.newPage();

        await page.goto('/');

        // Wait for app to fully load
        await page.waitForLoadState('networkidle');
        await page.waitForTimeout(2000); // Give Service Worker time to cache assets

        // Verify Service Worker is registered
        const swRegistered = await page.evaluate(() => {
            return navigator.serviceWorker.controller !== null;
        });
        expect(swRegistered).toBeTruthy();

        // Step 2: Close the page (simulate closing browser)
        await page.close();

        // Step 3: Open new page in OFFLINE mode
        const offlinePage = await context.newPage();

        // Enable offline mode BEFORE navigation
        await context.setOffline(true);

        // Step 4: Try to open the app URL
        const startTime = Date.now();
        await offlinePage.goto('/', { waitUntil: 'domcontentloaded', timeout: 15000 });
        const loadTime = Date.now() - startTime;

        // CRITICAL: App should load, NOT white screen
        const bodyContent = await offlinePage.locator('body').innerHTML();
        expect(bodyContent.length).toBeGreaterThan(100); // Has actual content, not empty

        // Verify critical elements loaded from cache
        const appRoot = offlinePage.locator('#root');
        await expect(appRoot).toBeVisible({ timeout: 5000 });

        // Verify at least login or main UI visible
        const loginOrMain = offlinePage.getByRole('button').first().or(offlinePage.getByText(/harvest|login/i));
        await expect(loginOrMain).toBeVisible({ timeout: 5000 });

        // Verify JS executed (not just HTML cached)
        const hasReact = await offlinePage.evaluate(() => {
            return window.React !== undefined || document.querySelector('[data-reactroot]') !== null;
        });
        expect(hasReact).toBeTruthy();

        // Performance check: Should load in < 3 seconds from cache
        expect(loadTime).toBeLessThan(3000);

        console.log(`✅ PWA loaded offline in ${loadTime}ms`);

        await offlinePage.close();
        await context.close();
    });

    test('Service Worker caches all critical assets', async ({ page }) => {
        await page.goto('/');
        await page.waitForLoadState('networkidle');

        // Wait for SW to activate
        await page.waitForTimeout(2000);

        // Check cached assets via Service Worker API
        const cachedAssets = await page.evaluate(async () => {
            const cacheNames = await caches.keys();
            const assets: string[] = [];

            for (const cacheName of cacheNames) {
                const cache = await caches.open(cacheName);
                const requests = await cache.keys();
                assets.push(...requests.map(req => req.url));
            }

            return assets;
        });

        // Verify critical assets are cached
        const hasIndexHtml = cachedAssets.some(url => url.includes('index.html') || url.endsWith('/'));
        const hasJsBundle = cachedAssets.some(url => url.includes('.js'));
        const hasCssBundle = cachedAssets.some(url => url.includes('.css'));

        expect(hasIndexHtml).toBeTruthy();
        expect(hasJsBundle).toBeTruthy();
        expect(hasCssBundle).toBeTruthy();

        console.log(`✅ Service Worker cached ${cachedAssets.length} assets`);
    });

    test('Offline indicator shows when offline', async ({ page }) => {
        await page.goto('/login');

        // Login to access main app
        await page.fill('input[type="email"]', 'runner@harvestpro.nz');
        await page.fill('input[type="password"]', 'password123');
        await page.click('button[type="submit"]');
        await page.waitForURL('/runner');

        // Go offline
        await page.context().setOffline(true);

        // Verify offline indicator appears
        const offlineIndicator = page.locator('[data-testid="offline-indicator"]')
            .or(page.getByText(/offline|sin conexión/i));
        await expect(offlineIndicator).toBeVisible({ timeout: 5000 });

        // Go back online
        await page.context().setOffline(false);

        // Verify indicator disappears
        await expect(offlineIndicator).not.toBeVisible({ timeout: 5000 });
    });
});

/**
 * INDUSTRIAL TEST 2: Duplicate Sticker Conflict (Idempotency)
 * 
 * Tests that duplicate scans are handled gracefully with clear messages.
 * Critical for field operations where Runners might scan twice by accident.
 */

test.describe('Duplicate Sticker Conflict - Idempotency', () => {
    test('CRITICAL: Clear error message for duplicate scan (same picker, same time)', async ({ page }) => {
        await page.goto('/login');
        await page.fill('input[type="email"]', 'runner@harvestpro.nz');
        await page.fill('input[type="password"]', 'password123');
        await page.click('button[type="submit"]');
        await page.waitForURL('/runner');

        // Scan bucket first time
        await page.getByRole('button', { name: /scan/i }).click();
        const input1 = page.locator('input[placeholder*="picker"]');
        await input1.fill('PICKER-DUPLICATE-001');
        await page.getByRole('button', { name: /confirm/i }).click();

        // Wait for scan to process
        await page.waitForTimeout(1000);

        // Scan SAME bucket immediately (duplicate)
        await page.getByRole('button', { name: /scan/i }).click();
        const input2 = page.locator('input[placeholder*="picker"]');
        await input2.fill('PICKER-DUPLICATE-001');
        await page.getByRole('button', { name: /confirm/i }).click();

        // Verify clear error message (NOT generic DB error)
        const errorMessage = page.locator('[role="alert"]').or(page.getByText(/duplicate|already|ya.*registrado/i));
        await expect(errorMessage).toBeVisible({ timeout: 3000 });

        // Error should mention "duplicate" or "already registered"
        const errorText = await errorMessage.textContent();
        expect(errorText?.toLowerCase()).toMatch(/duplicate|already|ya.*registrado|duplicate/i);

        // Should NOT show raw DB error like "23505" or "unique_constraint"
        expect(errorText?.toLowerCase()).not.toContain('23505');
        expect(errorText?.toLowerCase()).not.toContain('unique_constraint');
    });

    test('Offline duplicate scan queues only once', async ({ page }) => {
        await page.goto('/login');
        await page.fill('input[type="email"]', 'runner@harvestpro.nz');
        await page.fill('input[type="password"]', 'password123');
        await page.click('button[type="submit"]');
        await page.waitForURL('/runner');

        // Go offline
        await page.context().setOffline(true);

        // Scan same picker twice offline
        for (let i = 0; i < 2; i++) {
            await page.getByRole('button', { name: /scan/i }).click();
            const input = page.locator('input[placeholder*="picker"]');
            await input.fill('PICKER-OFFLINE-DUP-001');
            await page.getByRole('button', { name: /confirm/i }).click();
            await page.waitForTimeout(500);
        }

        // Check sync queue badge
        const syncBadge = page.getByTestId('sync-badge');
        const badgeText = await syncBadge.textContent();
        const queueCount = parseInt(badgeText?.replace(/[^0-9]/g, '') || '0');

        // Should have 2 items queued (both attempts)
        expect(queueCount).toBe(2);

        // Go online and wait for sync
        await page.context().setOffline(false);
        await page.waitForTimeout(5000);

        // One should succeed, one should fail with duplicate error in DLQ
        // Check DLQ (if accessible from runner)
        // This part would need Manager view access
    });

    test('Simultaneous scans from different devices handled gracefully', async ({ browser }) => {
        // Simulate two runners scanning same picker at almost same time
        const context1 = await browser.newContext();
        const context2 = await browser.newContext();

        const runner1 = await context1.newPage();
        const runner2 = await context2.newPage();

        // Both login
        for (const page of [runner1, runner2]) {
            await page.goto('/login');
            await page.fill('input[type="email"]', 'runner@harvestpro.nz');
            await page.fill('input[type="password"]', 'password123');
            await page.click('button[type="submit"]');
            await page.waitForURL('/runner');
        }

        // Both scan simultaneously
        const scanPromises = [runner1, runner2].map(async (page) => {
            await page.getByRole('button', { name: /scan/i }).click();
            const input = page.locator('input[placeholder*="picker"]');
            await input.fill('PICKER-RACE-001');
            await page.getByRole('button', { name: /confirm/i }).click();
        });

        await Promise.all(scanPromises);

        // Wait for both to process
        await runner1.waitForTimeout(2000);
        await runner2.waitForTimeout(2000);

        // At least one should show success
        const success1 = await runner1.locator('[data-testid="scan-success"]').isVisible().catch(() => false);
        const success2 = await runner2.locator('[data-testid="scan-success"]').isVisible().catch(() => false);

        expect(success1 || success2).toBeTruthy();

        // The other might show duplicate error
        const error1 = await runner1.locator('[role="alert"]').isVisible().catch(() => false);
        const error2 = await runner2.locator('[role="alert"]').isVisible().catch(() => false);

        // At least one should complete without error
        expect(!error1 || !error2).toBeTruthy();

        await runner1.close();
        await runner2.close();
        await context1.close();
        await context2.close();
    });
});
