# State Management Architecture

**Last Updated**: 2026-02-11  
**Status**: Active  
**Pattern**: Hybrid (React Context + Zustand)

---

## Overview

HarvestPro NZ uses a **hybrid state management approach** combining React Context API and Zustand. Each tool is used for its strengths to optimize both developer experience and application performance.

---

## Current Architecture

### 1. React Context API

Used for **global, infrequent state** with complex side effects.

#### [`AuthContext`](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/context/AuthContext.tsx)

**Purpose**: Authentication and user session management  
**State**: `appUser`, `orchardId`, `isAuthenticated`, `isLoading`  
**Key Actions**: `signIn`, `signOut`, `loadUserData`, `switchOrchard`

**Why Context?**

- Authentication changes infrequently
- Needed globally across the app
- Avoids prop drilling
- Non-performance-critical

#### [`MessagingContext`](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/context/MessagingContext.tsx)

**Purpose**: Real-time messaging and chat  
**State**: `messages`, `groups`, `unreadCount`, `userId`, `orchardId`  
**Key Actions**: `sendMessage`, `markAsRead`, `refreshMessages`

**Why Context?**

- Complex real-time subscriptions (Supabase)
- Moderate update frequency
- Tightly coupled with authentication

### 2. Zustand Store

Used for **frequent updates** and **performance-critical state**.

#### [`useHarvestStore`](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/stores/useHarvestStore.ts)

**Purpose**: Harvest production data and real-time operations  
**State**: `buckets`, `crew`, `settings`, `stats`, `bins`  
**Size**: ~735 lines, 30+ actions  
**Key Actions**: `addBucket`, `updateSettings`, `fetchGlobalData`

**Why Zustand?**

- **High-frequency updates** (bucket scans every few seconds)
- **Performance-critical** (real-time production tracking)
- **localStorage persistence** (offline support)
- **Simple, predictable updates**

---

## Decision Guide: Context vs Zustand

### Use React Context when

✅ **Infrequent changes**  
Example: User authentication, theme, app configuration

✅ **Deep component tree access**  
Example: Avoiding prop drilling for auth or locale

✅ **Complex side effects**  
Example: Real-time subscriptions, WebSocket connections

✅ **Not performance-critical**  
Example: User profile data, settings that change rarely

### Use Zustand when

✅ **Frequent updates**  
Example: Real-time data, user interactions, animations

✅ **Performance is critical**  
Example: Large lists, high-frequency renders, dashboards

✅ **Need persistence**  
Example: localStorage sync, offline-first data

✅ **Simple state updates**  
Example: CRUD operations, counters, toggles

---

## Architecture Validation

| State Provider | Type | Frequency | Performance | Persistence | ✅ Correct Choice |
|---------------|------|-----------|-------------|-------------|------------------|
| `AuthContext` | Context | Low | Low priority | No | ✅ Yes |
| `MessagingContext` | Context | Medium | Medium priority | No | ✅ Yes |
| `useHarvestStore` | Zustand | High | Critical | Yes | ✅ Yes |

---

## Code Examples

### Accessing Auth Context

```tsx
import { useAuth } from '@/context';

function MyComponent() {
  const { appUser, signOut, orchardId } = useAuth();

  if (!appUser) {
    return <LoginPrompt />;
  }

  return (
    <div>
      <p>Welcome, {appUser.full_name}</p>
      <p>Orchard: {orchardId}</p>
      <button onClick={signOut}>Sign Out</button>
    </div>
  );
}
```

### Accessing Messaging Context

```tsx
import { useMessaging } from '@/context';

function ChatComponent() {
  const { messages, sendMessage, unreadCount } = useMessaging();

  const handleSend = async () => {
    await sendMessage(recipientId, messageContent);
  };

  return (
    <div>
      <Badge count={unreadCount} />
      <MessageList messages={messages} />
    </div>
  );
}
```

### Accessing Harvest Store

```tsx
import { useHarvestStore } from '@/stores/useHarvestStore';

function DashboardComponent() {
  const { stats, buckets, addBucket } = useHarvestStore();

  const handleScan = async (scan) => {
    addBucket({
      picker_id: scan.pickerId,
      quality_grade: scan.grade,
      orchard_id: orchardId,
    });
  };

  return (
    <div>
      <StatCard label="Total Buckets" value={stats.totalBuckets} />
      <StatCard label="Pay Estimate" value={stats.payEstimate} />
    </div>
  );
}
```

---

## Migration Guidelines

### When to migrate FROM Context TO Zustand

Consider migration if:

- Component re-renders are causing performance issues
- State updates happen more than 10 times per minute
- You need offline persistence
- State logic is getting complex (>200 lines)

### When to migrate FROM Zustand TO Context

Consider migration if:

- State updates are rare (< once per minute)
- You need complex side effects (subscriptions, cleanup)
- State is tightly coupled with authentication
- Performance is not a concern

---

## Future Considerations

### Potential Refactors

1. **Extract Settings from useHarvestStore**  
   Settings change infrequently - could move to Context or separate Zustand store

2. **Split useHarvestStore by Feature**  
   Consider splitting into:
   - `useBucketStore` (buckets, scans)
   - `useCrewStore` (crew, pickers, attendance)
   - `useStatsStore` (stats, calculations)

3. **Add Server State Management**  
   For complex server data, consider React Query or SWR

---

## References

- [React Context Docs](https://react.dev/reference/react/useContext)
- [Zustand Docs](https://docs.pmnd.rs/zustand/getting-started/introduction)
- [ADR-001: Hybrid State Management](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/docs/architecture/adr-001-hybrid-state-management.md)

---

**Questions?** Contact the development team or see ADR-001 for the full decision rationale.
