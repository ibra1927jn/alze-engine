# ADR-001: Hybrid State Management (Context + Zustand)

**Date**: 2026-02-11  
**Status**: ✅ Accepted  
**Deciders**: Development Team  
**Tags**: architecture, state-management, performance

---

## Context

HarvestPro NZ is a real-time harvest management application with diverse state management needs:

### Requirements

1. **Authentication State** - User sessions, roles, orchard selection
2. **Messaging State** - Real-time chat with Supabase subscriptions  
3. **Harvest Operations** - High-frequency bucket scans, crew tracking, production stats
4. **Offline Support** - localStorage persistence for critical data
5. **Performance** - Dashboard must render without lag despite frequent updates

### Constraints

- Must support offline-first workflows for field workers
- Real-time updates via Supabase subscriptions
- Bundle size should remain under 500KB gzipped
- Developer experience should be simple and predictable

---

## Decision

**We will use a hybrid approach combining React Context API and Zustand:**

### React Context for

- **Authentication** (`AuthContext`)
- **Messaging** (`MessagingContext`)

### Zustand for

- **Harvest Operations** (`useHarvestStore`)

---

## Rationale

### Why Hybrid Instead of Single Solution?

#### Option 1: All React Context ❌

**Rejected** because:

- Context causes re-renders of all consumers on any state change
- Performance issues with high-frequency harvest data updates
- No built-in persistence mechanism
- Complex optimization (useMemo, useCallback) required

#### Option 2: All Zustand ✅ (Partial)

**Partially accepted** because:

- ✅ Excellent performance for frequent updates
- ✅ Built-in persistence support
- ✅ Minimal boilerplate
- ❌ Less idiomatic for authentication patterns
- ❌ Requires workarounds for complex side effects (subscriptions)

#### Option 3: Hybrid (Context + Zustand) ✅ **CHOSEN**

**Accepted** because:

- ✅ Each tool used for its strengths
- ✅ Context excels at infrequent, global state
- ✅ Zustand excels at frequent, performance-critical state
- ✅ Clear decision boundaries for future development
- ⚠️ Two state systems to learn (acceptable tradeoff)

---

## Implementation Details

### AuthContext (React Context)

```tsx
// Why Context:
// - Authentication changes rarely (login/logout/orchard switch)
// - Needed globally (routes, permissions, UI)
// - Non-performance-critical
// - Tightly coupled with Supabase auth lifecycle

const AuthProvider: React.FC = ({ children }) => {
  const [state, setState] = useState<AuthState>({
    appUser: null,
    isAuthenticated: false,
    isLoading: true,
    orchardId: null,
  });

  // Complex side effects for auth lifecycle
  useEffect(() => {
    const { data: authListener } = supabase.auth.onAuthStateChange(...);
    return () => authListener?.subscription.unsubscribe();
  }, []);

  return <AuthContext.Provider value={...}>{children}</AuthContext.Provider>;
};
```

### MessagingContext (React Context)

```tsx
// Why Context:
// - Medium update frequency
// - Complex real-time subscriptions (Supabase)
// - Tightly coupled with AuthContext
// - Moderate performance requirements

const MessagingProvider: React.FC = ({ children }) => {
  const [state, setState] = useState<MessagingState>({
    messages: [],
    groups: [],
    unreadCount: 0,
  });

  // Real-time subscription management
  useEffect(() => {
    const subscription = supabase
      .channel('messages')
      .on('postgres_changes', ...)
      .subscribe();

    return () => subscription.unsubscribe();
  }, [userId, orchardId]);

  return <MessagingContext.Provider value={...}>{children}</MessagingContext.Provider>;
};
```

### useHarvestStore (Zustand)

```tsx
// Why Zustand:
// - High-frequency updates (bucket scans every few seconds)
// - Performance-critical (real-time dashboard)
// - localStorage persistence (offline support)
// - Simple, predictable state updates

export const useHarvestStore = create<HarvestState>()(
  persist(
    (set, get) => ({
      buckets: [],
      stats: { totalBuckets: 0, payEstimate: 0 },

      // High-frequency action optimized with Zustand
      addBucket: (bucket) => set((state) => ({
        buckets: [...state.buckets, bucket],
        stats: calculateStats([...state.buckets, bucket]),
      })),

      // Fetch and sync without causing global re-renders
      fetchGlobalData: async () => {
        const data = await fetchFromSupabase();
        set({ buckets: data.buckets, crew: data.crew });
      },
    }),
    { name: 'harvest-store' } // localStorage persistence
  )
);
```

---

## Consequences

### Positive

✅ **Optimal Performance**  

- Context for infrequent updates (auth, messaging)
- Zustand for high-frequency updates (harvest operations)

✅ **Clear Patterns**  

- Decision tree documented for future state additions
- Each tool used for its strengths

✅ **Offline Support**  

- Zustand persistence for critical harvest data
- Context manages online/offline auth state

✅ **Developer Experience**  

- Familiar Context API for auth patterns
- Simple Zustand hooks for data operations

### Negative

⚠️ **Learning Curve**  

- Developers must understand **two** state systems
- Decision required: "Should this be Context or Zustand?"

⚠️ **Potential Overengineering**  

- Small apps might not need hybrid approach
- HarvestPro's scale justifies complexity

### Neutral

🔄 **Migration Path Exists**  

- Can migrate Context → Zustand if performance becomes issue
- Can migrate Zustand → Context if side effects become complex

---

## Alternatives Considered

### Redux Toolkit

**Rejected** because:

- ❌ More boilerplate than Zustand
- ❌ Overkill for our use case
- ❌ Larger bundle size
- ✅ Would work, but offers no advantage over Zustand

### Jotai / Recoil (Atomic State)

**Rejected** because:

- ❌ Less mature ecosystem
- ❌ Atomic pattern doesn't fit our domain model
- ❌ Team unfamiliarity
- ✅ Interesting for future exploration

### MobX

**Rejected** because:

- ❌ Decorator syntax requires build configuration
- ❌ Magic (observables) can be confusing
- ❌ Less popular in React ecosystem
- ✅ Would work, but team prefers explicit over implicit

---

## Future Review Triggers

This decision should be revisited if:

1. **Performance degrades** in MessagingContext (consider Zustand migration)
2. **Authentication becomes complex** (consider Redux/Zustand with middleware)
3. **useHarvestStore exceeds 1000 lines** (consider splitting into feature stores)
4. **Team size grows beyond 5 developers** (may need more structure)
5. **Bundle size exceeds 500KB** (reevaluate all dependencies)

---

## References

- [React Context Best Practices](https://react.dev/learn/passing-data-deeply-with-context)
- [Zustand Performance Guide](https://docs.pmnd.rs/zustand/guides/performance)
- [State Management Comparison](https://2023-state-of-js-results/)
- [State Management Architecture Guide](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/docs/architecture/state-management.md)

---

**Decision Owner**: Development Team  
**Review Date**: 2026-08-11 (6 months)
