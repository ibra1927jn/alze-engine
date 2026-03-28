// =============================================
// E2E: LOGIN FLOW TESTS FOR ALL 3 ROLES
// =============================================
import { test, expect } from '@playwright/test';

// Uses baseURL from playwright.config.ts (http://localhost:5173)
const BASE = '';

// Test credentials — match demo accounts from seed data
const ROLES = {
    manager: { email: 'manager@harvestpro.nz', password: '111111', expectedUrl: '/manager', label: 'Manager' },
    teamLeader: { email: 'lead@harvestpro.nz', password: '111111', expectedUrl: '/team-leader', label: 'Team Leader' },
    runner: { email: 'runner@harvestpro.nz', password: '111111', expectedUrl: '/runner', label: 'Bucket Runner' },
};

test.describe('Login Flow - All Roles', () => {
    // =============================================
    // BASIC NAVIGATION
    // =============================================
    test('Login page loads correctly', async ({ page }) => {
        await page.goto(`${BASE}/login`);

        // Should have email and password fields
        await expect(page.locator('input[type="email"]')).toBeVisible();
        await expect(page.locator('input[type="password"]')).toBeVisible();

        // Should have a submit button
        await expect(page.locator('button[type="submit"]')).toBeVisible();
    });

    test('Empty form shows validation', async ({ page }) => {
        await page.goto(`${BASE}/login`);
        await page.click('button[type="submit"]');

        // Should stay on login page
        await expect(page).toHaveURL(/login/);
    });

    test('Invalid credentials show error', async ({ page }) => {
        await page.goto(`${BASE}/login`);

        await page.fill('input[type="email"]', 'invalid@test.com');
        await page.fill('input[type="password"]', 'wrongpassword');
        await page.click('button[type="submit"]');

        // Should show error or stay on login
        await page.waitForTimeout(3000);
        const url = page.url();
        // Should not redirect to a dashboard
        expect(url).not.toMatch(/\/(manager|team-leader|runner)/);
    });

    // =============================================
    // ROLE-SPECIFIC LOGIN
    // =============================================
    test('Manager login redirects to manager dashboard', async ({ page }) => {
        await page.goto(`${BASE}/login`);

        await page.fill('input[type="email"]', ROLES.manager.email);
        await page.fill('input[type="password"]', ROLES.manager.password);
        await page.click('button[type="submit"]');

        // Wait for redirect (MFA or dashboard)
        await page.waitForURL(/\/(manager|mfa)/, { timeout: 10000 });

        const url = page.url();
        expect(url).toMatch(/\/(manager|mfa)/);
    });

    test('Team Leader login redirects to team-leader dashboard', async ({ page }) => {
        await page.goto(`${BASE}/login`);

        await page.fill('input[type="email"]', ROLES.teamLeader.email);
        await page.fill('input[type="password"]', ROLES.teamLeader.password);
        await page.click('button[type="submit"]');

        // Wait for redirect
        await page.waitForURL(/\/team-leader/, { timeout: 10000 });

        await expect(page).toHaveURL(/\/team-leader/);
    });

    test('Runner login redirects to runner dashboard', async ({ page }) => {
        await page.goto(`${BASE}/login`);

        await page.fill('input[type="email"]', ROLES.runner.email);
        await page.fill('input[type="password"]', ROLES.runner.password);
        await page.click('button[type="submit"]');

        // Wait for redirect
        await page.waitForURL(/\/runner/, { timeout: 10000 });

        await expect(page).toHaveURL(/\/runner/);
    });

    // =============================================
    // PAGE CONTENT VERIFICATION
    // =============================================
    test('Team Leader dashboard shows content', async ({ page }) => {
        await page.goto(`${BASE}/login`);

        await page.fill('input[type="email"]', ROLES.teamLeader.email);
        await page.fill('input[type="password"]', ROLES.teamLeader.password);
        await page.click('button[type="submit"]');

        await page.waitForURL(/\/team-leader/, { timeout: 10000 });

        // Should show team leader specific content
        const bodyText = await page.textContent('body');
        expect(bodyText).toBeTruthy();
    });

    test('Runner dashboard shows Logistics Hub', async ({ page }) => {
        await page.goto(`${BASE}/login`);

        await page.fill('input[type="email"]', ROLES.runner.email);
        await page.fill('input[type="password"]', ROLES.runner.password);
        await page.click('button[type="submit"]');

        await page.waitForURL(/\/runner/, { timeout: 10000 });

        // Should show Logistics Hub
        await expect(page.getByText(/Logistics|Hub|Scan/i).first()).toBeVisible({ timeout: 5000 });
    });

    // =============================================
    // AUTH GUARDS
    // =============================================
    test('Unauthenticated access to /manager redirects to login', async ({ page }) => {
        // Clear all cookies/storage
        await page.context().clearCookies();

        await page.goto(`${BASE}/manager`);

        // Should redirect to login
        await page.waitForTimeout(3000);
        const url = page.url();
        expect(url).toMatch(/\/(login|$)/);
    });

    test('Unauthenticated access to /runner redirects to login', async ({ page }) => {
        await page.context().clearCookies();

        await page.goto(`${BASE}/runner`);

        await page.waitForTimeout(3000);
        const url = page.url();
        expect(url).toMatch(/\/(login|$)/);
    });

    test('Unauthenticated access to /team-leader redirects to login', async ({ page }) => {
        await page.context().clearCookies();

        await page.goto(`${BASE}/team-leader`);

        await page.waitForTimeout(3000);
        const url = page.url();
        expect(url).toMatch(/\/(login|$)/);
    });
});
