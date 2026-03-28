import { chromium } from 'playwright';
import { createClient } from '@supabase/supabase-js';
import dotenv from 'dotenv';
dotenv.config();

// Config
const APP_URL = 'http://localhost:3000/runner'; // Ensure app is running!
const SUPABASE_URL = process.env.VITE_SUPABASE_URL;
const SUPABASE_KEY = process.env.VITE_SUPABASE_ANON_KEY;

if (!SUPABASE_URL || !SUPABASE_KEY) {
    console.error("❌ Missing Supabase Env Vars");
    process.exit(1);
}

const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);

(async () => {
    console.log("🚀 Starting Stress Test Runner (Offline Logic)...");

    let browser;
    try {
        // Launch Browser
        browser = await chromium.launch({ headless: false });
        const context = await browser.newContext();
        const page = await context.newPage();

        // Debug Browser Logs
        page.on('console', msg => {
            const type = msg.type();
            if (type === 'error' || type === 'warning' || msg.text().includes('[Offline]')) {
                console.log(`[Browser Console] ${type.toUpperCase()}: ${msg.text()}`);
            }
        });

        // 0. Auth Setup
        const email = `stress.tester.${Date.now()}@example.com`;
        const password = 'Password123!';
        console.log(`[Setup] Creating user: ${email}`);

        // A) Sign Up & Initial Role (Manager) to setup data
        const { data: authData, error: authError } = await supabase.auth.signUp({ email, password });
        if (authError) throw new Error("Auth Failed: " + authError.message);
        const userId = authData.user?.id;
        if (!userId) throw new Error("Auth Failed: No User ID");

        // Fetch Orchard
        const { data: orchards } = await supabase.from('orchards').select('id').limit(1);
        const orchardId = orchards[0].id;

        // Ensure User is Manager first
        await supabase.from('users').upsert({
            id: userId,
            email: email,
            role: 'manager',
            orchard_id: orchardId,
            full_name: 'Stress Tester'
        });

        // B) Create Picker (As Manager)
        const pickerId = `stress-test-${Date.now()}`;
        console.log(`[Setup] Creating picker: ${pickerId}`);
        const { error: pickerError } = await supabase.from('pickers').insert([{
            picker_id: pickerId,
            name: 'Stress Tester',
            orchard_id: orchardId,
            current_row: 999
        }]);
        if (pickerError) throw new Error("Failed to create picker: " + pickerError.message);

        // C) Demote to Runner (For Test Access)
        console.log("[Setup] Demoting user to Runner...");
        await supabase.from('users').update({ role: 'runner' }).eq('id', userId);


        // 1. Browser Login
        console.log(`[Browser] Navigating to Login...`);
        await page.goto('http://localhost:3000/login'); // Go to Login directly

        await page.fill('input[type="email"]', email);
        await page.fill('input[type="password"]', password);
        await page.click('button[type="submit"]');

        console.log("[Browser] Logging in...");

        // Wait for redirect to /runner
        try {
            await page.waitForURL('**/runner', { timeout: 15000 });
            console.log("[Browser] Login Successful. Initialized on /runner.");
        } catch {
            console.error("❌ Login Timeout. Check URL or Credentials.");
            throw new Error("Login Failed");
        }

        // Helper: Access IndexedDB (Existing Code...)
        const getPendingCount = async () => {
            return await page.evaluate(async () => {
                return new Promise((resolve, reject) => {
                    const req = indexedDB.open('HarvestProDB');
                    req.onsuccess = (e) => {
                        const db = e.target.result;
                        if (!db.objectStoreNames.contains('bucket_queue')) { resolve(0); return; }
                        const tx = db.transaction(['bucket_queue'], 'readonly');
                        const store = tx.objectStore('bucket_queue');
                        const index = store.index('synced');
                        const countReq = index.count(IDBKeyRange.only(0));
                        countReq.onsuccess = () => resolve(countReq.result);
                        countReq.onerror = () => reject(countReq.error);
                    };
                    req.onerror = () => reject(req.error);
                });
            });
        };

        // --- STEP 1: CHAOS (OFFLINE) ---
        console.log("\n🌪️ PHASE 1: SIMULATING NETWORK CHAOS (OFFLINE)...");
        await context.setOffline(true);
        console.log("[Browser] Offline Mode: ON");

        // Scan 5 times
        console.log("[Action] Performing 5 Scans...");
        for (let i = 0; i < 5; i++) {
            // 1. Open Scanner
            if (i === 0) await page.waitForSelector('button:has-text("Scan Sticker")');
            await page.getByText('Scan Sticker').click();

            // 2. Switch to Manual Entry (Scanner defaults to camera)
            // Wait for modal to appear
            await page.waitForSelector('button:has-text("Enter Code Manually")', { timeout: 5000 });
            await page.getByText('Enter Code Manually').click();

            // 3. Fill Input
            const inputSelector = 'input[placeholder="Enter BUCKET code..."]';
            await page.waitForSelector(inputSelector);
            await page.fill(inputSelector, pickerId);

            // 4. Submit
            await page.getByText('Submit Code').click();

            // 5. Wait for modal to close (or close it?)
            // handleScanComplete closes modal.
            // Wait a tiny bit for state update
            await page.waitForTimeout(500);
        }

        // Validate Queue
        const pendingCount = await getPendingCount();
        console.log(`[Validation] Offline Queue Count: ${pendingCount}`);

        if (pendingCount === 5) {
            console.log("✅ Phase 1 Passed: 5 items queued offline.");
        } else {
            console.error(`❌ Phase 1 Failed: Expected 5, got ${pendingCount}`);
        }

        // --- STEP 2: RECOVERY (ONLINE) ---
        console.log("\n🌤️ PHASE 2: SIMULATING RECOVERY (ONLINE)...");
        await context.setOffline(false);
        console.log("[Browser] Offline Mode: OFF");

        // Wait for Sync (Polling UI or DB)
        console.log("[Action] Waiting for Sync...");

        await page.waitForFunction(async () => {
            return await new Promise((resolve) => {
                const req = indexedDB.open('HarvestProDB');
                req.onsuccess = (e) => {
                    const db = e.target.result;
                    const tx = db.transaction(['bucket_queue'], 'readonly');
                    const store = tx.objectStore('bucket_queue');
                    const index = store.index('synced');
                    const countReq = index.count(IDBKeyRange.only(0));
                    countReq.onsuccess = () => resolve(countReq.result === 0);
                };
            });
        }, null, { timeout: 30000 });

        console.log("[Browser] Sync Completed (Queue Empty).");

        // Verify Server DB
        const { data: pickerData } = await supabase.from('pickers').select('id').eq('picker_id', pickerId).single();
        const pickerUUID = pickerData?.id;

        const { count: serverCount } = await supabase
            .from('bucket_records')
            .select('*', { count: 'exact', head: true })
            .eq('picker_id', pickerUUID);

        console.log(`[Validation] Server Record Count: ${serverCount}`);

        if (serverCount >= 5) {
            console.log("✅ Phase 2 Passed: Records synced to Supabase.");
        } else {
            console.error(`❌ Phase 2 Failed: Expected 5+, got ${serverCount}`);
        }

        // --- STEP 3: CORRUPT CODE TEST ---
        console.log("\n🧪 PHASE 3: CORRUPT CODE TEST...");
        const badCode = "INVALID-CODE-999";

        // Listen for alert dialog
        let alertMessage = null;
        page.on('dialog', async dialog => {
            alertMessage = dialog.message();
            await dialog.accept();
        });

        await page.getByText('Scan Sticker').click();
        await page.fill('input[type="text"]', badCode);
        await page.click('button:has-text("Simulate Scan")');

        await page.waitForTimeout(1000);

        console.log(`[Validation] Alert Message: "${alertMessage}"`);

        if (alertMessage && (alertMessage.includes('Invalid') || alertMessage.includes('Error'))) {
            console.log("✅ Phase 3 Passed: Correct Error Alert shown.");
        } else {
            console.error(`❌ Phase 3 Failed: Alert message mismatch or missing.`);
        }

        // Verify Queue is Clean (Should be 0)
        const finalQueue = await getPendingCount();
        if (finalQueue === 0) {
            console.log("✅ Phase 3 Passed: Invalid scan was NOT queued.");
        } else {
            console.error(`❌ Phase 3 Failed: Invalid scan WAS queued (Count: ${finalQueue})`);
        }

        console.log("\n🎉 STRESS TEST COMPLETED.");

    } catch (e) {
        console.error("❌ Test Failed:", e);
    } finally {
        if (browser) await browser.close();
    }
})();
