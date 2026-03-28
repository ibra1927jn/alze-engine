/**
 * UI Primitives — Barrel Export
 *
 * Pure, reusable UI components with ZERO business logic.
 * These never import from @/services, @/hooks, @/stores, or @/repositories.
 *
 * Usage:
 *   import { Button, StatusBadge, StatCard } from '@/components/ui';
 */

// Layout & Structure
export { default as PageHeader } from './PageHeader';
export { default as ModalOverlay } from './ModalOverlay';
export { default as Drawer } from './Drawer';

// Data Display
export { default as StatCard } from './StatCard';
export { default as StatusBadge } from './StatusBadge';
export { default as EmptyState } from './EmptyState';
export { default as LoadingSkeleton } from './LoadingSkeleton';
export { default as VirtualList } from './VirtualList';

// Navigation & Input
export { default as TabGroup } from './TabGroup';
export type { Tab } from './TabGroup';
export { default as Button } from './Button';
export { default as FilterBar } from './FilterBar';
export { default as InlineEdit } from './InlineEdit';
export { default as InlineSelect } from './InlineSelect';

// Feedback & Icons
export { default as Toast } from './Toast';
export { default as Icon } from './Icon';

// Error Handling
export { default as ComponentErrorBoundary } from './ComponentErrorBoundary';
