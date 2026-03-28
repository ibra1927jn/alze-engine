/**
 * Common Components — Barrel Export
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  UI primitives live in @/components/ui/ now.                    │
 * │  This file re-exports them for backward compatibility.          │
 * │  New code should import directly from '@/components/ui'.        │
 * └─────────────────────────────────────────────────────────────────┘
 */

// Re-export all UI primitives for backward compatibility
export {
    PageHeader,
    ModalOverlay,
    Drawer,
    StatCard,
    StatusBadge,
    EmptyState,
    LoadingSkeleton,
    VirtualList,
    TabGroup,
    Button,
    FilterBar,
    InlineEdit,
    InlineSelect,
    Toast,
    Icon,
    ComponentErrorBoundary,
} from '../ui';
export type { Tab } from '../ui';

// Feature components (domain-aware — NOT in ui/)
export { default as DesktopLayout } from './DesktopLayout';
export type { NavItem } from './DesktopLayout';
export { default as ErrorBoundary } from './ErrorBoundary';
export { default as Header } from './Header';
export { default as BottomNav } from './BottomNav';
export { default as NotificationPanel } from './NotificationPanel';
export { default as ConflictResolver } from './ConflictResolver';
export { default as HarvestSyncBridge } from './HarvestSyncBridge';
export { default as PickerProfileDrawer } from './PickerProfileDrawer';
export { default as PwaInstallBanner } from './PwaInstallBanner';
export { default as SetupWizard } from './SetupWizard';
export { default as SyncStatusMonitor } from './SyncStatusMonitor';
export { default as TrustBadges } from './TrustBadges';
export { default as UnifiedMessagingView } from './UnifiedMessagingView';
