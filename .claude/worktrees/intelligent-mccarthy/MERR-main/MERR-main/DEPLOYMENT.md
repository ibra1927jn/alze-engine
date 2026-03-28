# Deployment Guide — HarvestPro NZ

## Prerequisites

- [Supabase CLI](https://supabase.com/docs/guides/local-development) installed and linked
- Node.js 20+ and npm
- Access to the Supabase project dashboard

---

## Step 1: Apply RLS Migration

The `20260301_rls_consolidation.sql` migration eliminates recursive RLS policies across 8 tables.

```bash
# Option A: Push via Supabase CLI
supabase db push

# Option B: Apply manually via Supabase dashboard
# Go to SQL Editor → paste contents of supabase/migrations/20260301_rls_consolidation.sql → Run
```

**Verify:** In the Supabase dashboard, check that the `is_manager()`, `is_role()`, `get_auth_role()`, and `get_auth_orchard_id()` functions exist under Database → Functions.

---

## Step 2: Deploy Edge Function (Fraud Detection)

```bash
# Deploy the fraud detection function
supabase functions deploy detect-anomalies

# Verify it's live
supabase functions list
```

**Monitor:** Check Edge Function logs for the first few days:

```bash
supabase functions logs detect-anomalies --scroll
```

> **Tuning:** If the 2.5× `maxPhysicalRate` threshold generates false positives,
> adjust it in `supabase/functions/detect-anomalies/index.ts` line with `MAX_PHYSICAL_RATE`.

---

## Step 3: Deploy Frontend

```bash
# Build production bundle
npm run build

# Preview locally before deploying
npm run preview

# Deploy to your hosting (Vercel, Netlify, etc.)
# Example for Vercel:
npx vercel --prod
```

---

## Step 4: Capacitor Native Builds (Optional)

### Initial Setup (one-time)

```bash
# Install Capacitor dependencies
npm install @capacitor/core @capacitor/cli @capacitor-community/barcode-scanner

# Initialize Capacitor
npx cap init

# Add platforms
npx cap add android
npx cap add ios

# Sync web assets
npx cap sync
```

### Build APK (Android)

```bash
npm run build
npx cap sync android
npx cap open android
# → Build → Generate Signed Bundle/APK in Android Studio
```

### Build IPA (iOS)

```bash
npm run build
npx cap sync ios
npx cap open ios
# → Product → Archive in Xcode
```

### Distribute

- **MDM:** Upload APK/IPA to your MDM solution for Team Leader devices
- **TestFlight:** Upload IPA to App Store Connect for iOS beta testing
- **Internal Track:** Upload APK to Google Play Console internal testing

---

## Post-Deployment Checklist

- [ ] RLS migration applied — verify functions in Supabase dashboard
- [ ] Edge Function deployed — verify with `supabase functions list`
- [ ] Frontend deployed — verify login + Manager dashboard loads
- [ ] Fraud Shield shows **Live** badge (green) instead of Demo
- [ ] Language switcher works in Settings (English → Español → Māori)
- [ ] Native scanner works on test device (if Capacitor build deployed)
