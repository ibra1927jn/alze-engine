import React from "react";
import { createRoot } from "react-dom/client";
import { RouterProvider } from "react-router-dom";
import { AppProvider } from "./context";
import { router } from "./routes";
import { MFAGuard } from './components/MFAGuard';
import { I18nProvider } from './i18n';
import "./index.css";

// Initialize Sentry for error tracking (must be before React)
import { initSentry } from './config/sentry';
initSentry();

// Initialize PostHog for analytics (after Sentry)
import { initPostHog } from './config/analytics';
initPostHog();

// 🔧 V16: Request persistent storage to prevent silent IndexedDB eviction on mobile
// Without this, iOS Safari and some Android browsers can silently delete Dexie data
// when storage pressure is high, causing total offline data loss.
if (navigator.storage?.persist) {
  navigator.storage.persist().then(granted => {
    if (!granted) {
      console.warn('[Storage] Persistent storage NOT granted — data may be evicted');
    }
  });
}

const container = document.getElementById("root");
if (container) {
  // Lazy import for PWA banner (code-split, not needed for initial render)
  const PwaInstallBanner = React.lazy(() => import('./components/common/PwaInstallBanner'));

  const root = createRoot(container);
  root.render(
    <I18nProvider>
      <AppProvider>
        <MFAGuard>
          <RouterProvider router={router} />
          <React.Suspense fallback={null}>
            <PwaInstallBanner />
          </React.Suspense>
        </MFAGuard>
      </AppProvider>
    </I18nProvider>
  );
}
