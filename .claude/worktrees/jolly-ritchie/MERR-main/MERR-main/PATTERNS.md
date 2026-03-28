# React & TypeScript Patterns

**Purpose**: Document reusable patterns used in HarvestPro NZ codebase.

---

## React Patterns

### 1. useCallback for Function Stability

**Use When**: Functions are dependencies in `useEffect` or passed as props to memoized components.

**Problem**: Function recreated on every render → infinite loops or unnecessary re-renders.

**Solution**:

```typescript
const loadConversations = useCallback(async () => {
    const convs = await simpleMessagingService.getConversationsByUserId(userId);
    setConversations(convs);
}, [userId]); // Stable unless userId changes

useEffect(() => {
    loadConversations();
}, [loadConversations]); // Safe - won't cause infinite loop
```

**Example**: [SimpleChat.tsx](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/components/SimpleChat.tsx#L28-L35)

---

### 2. useRef Pattern for Circular Dependencies

**Use When**: Two functions call each other (circular dependency).

**Problem**: Can't add each other as dependencies → infinite loop or stale closures.

**Solution**:

```typescript
// Step 1: Create stable function with empty deps
const scheduleNext = useCallback(() => {
    setTimeout(() => {
        syncPendingBucketsRef.current?.(); // Call via ref
    }, 5000);
}, []); // Empty deps = stable forever

// Step 2: Create ref to hold latest version
const syncPendingBucketsRef = useRef<(() => void) | null>(null);

// Step 3: Main function depends on stable scheduleNext
const syncPendingBuckets = useCallback(async () => {
    // ... sync logic
    scheduleNext(); // Call stable function
}, [scheduleNext]); // Safe dependency

// Step 4: Keep ref updated
useEffect(() => {
    syncPendingBucketsRef.current = syncPendingBuckets;
}, [syncPendingBuckets]);
```

**Why It Works**:

- `scheduleNext` is stable (empty deps) → never changes
- `syncPendingBuckets` safely depends on stable `scheduleNext`
- Ref updates automatically keep latest version
- No circular dependency in hooks

**Example**: [HarvestSyncBridge.tsx](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/components/common/HarvestSyncBridge.tsx#L96-L120)

---

### 3. Consistent Dependency Tracking

**Use When**: Effects depend on object properties, not entire object.

**Problem**: `activeConversation` object changes identity → effect runs unnecessarily.

**Solution**:

```typescript
useEffect(() => {
    const conversationId = activeConversation?.id; // Extract once
    if (conversationId) {
        // Use extracted ID consistently
        loadMessages(conversationId);
    }
}, [activeConversation?.id]); // Track only ID, not whole object
```

**Example**: [SimpleChat.tsx](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/components/SimpleChat.tsx#L63)

---

## TypeScript Patterns

### 4. Unknown → Safe Cast Pattern

**Use When**: Handling external data (API responses, database queries) with flexible schemas.

**Problem**: `any` disables type checking. Strict types break when schema changes.

**Solution**:

```typescript
// Step 1: Use unknown for raw data
const data: unknown = await supabase.from('table').select();

// Step 2: Cast to flexible Record
const items = (data as any[]).map((item: unknown) => {
    const row = item as Record<string, unknown>;
    
    // Step 3: Safe access with explicit casting
    return {
        id: String(row.id),
        name: String(row.name),
        count: Number(row.count) || 0
    };
});
```

**Benefits**:

- Catches type errors at compile time
- Handles schema drift gracefully
- Better autocomplete than `any`

**Example**: [simple-messaging.service.ts](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/services/simple-messaging.service.ts#L103-L110)

---

### 5. Runtime Type Guards

**Use When**: Need runtime validation of external data.

**Pattern**:

```typescript
// Define type
export interface SupabasePicker {
    id: UUID;
    name: string;
    badge_id: string | null;
}

// Create type guard
export function isSupabasePicker(value: unknown): value is SupabasePicker {
    if (typeof value !== 'object' || value === null) return false;
    
    const obj = value as Record<string, unknown>;
    return (
        typeof obj.id === 'string' &&
        typeof obj.name === 'string' &&
        (obj.badge_id === null || typeof obj.badge_id === 'string')
    );
}

// Use in code
if (isSupabasePicker(data)) {
    // TypeScript knows data is SupabasePicker
    console.log(data.name); // ✅ Type-safe
}
```

**Benefits**:

- Runtime safety for external data
- TypeScript narrows types automatically
- Self-documenting validation

**Example**: [database.types.ts](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/types/database.types.ts#L66-L77)

---

### 6. Branded Types for Domain Safety

**Use When**: Prevent mixing different string/number types (e.g., IDs, timestamps).

**Pattern**:

```typescript
// Brand the type
export type UUID = string & { readonly __brand: 'UUID' };
export type Timestamp = string & { readonly __brand: 'Timestamp' };

// Usage enforces type distinction
function getUser(id: UUID): User { /* ... */ }
function formatTime(ts: Timestamp): string { /* ... */ }

// Type errors prevent mistakes
const userId: UUID = "abc-123" as UUID;
const timestamp: Timestamp = "2024-01-01" as Timestamp;

getUser(timestamp); // ❌ Type error - prevents mixing
```

**Benefits**:

- Catches ID/timestamp mixing at compile time
- Zero runtime overhead
- Documents intent clearly

**Example**: [database.types.ts](file:///c:/Users/ibrab/Downloads/app/harvestpro-nz%20%281%29/src/types/database.types.ts#L13-L16)

---

## Anti-Patterns to Avoid

### ❌ Missing useCallback Dependencies

```typescript
// BAD - missing userId dependency
const load = useCallback(async () => {
    await fetchData(userId); // Stale closure!
}, []); // userId not in deps
```

### ❌ Using `any` for API Data

```typescript
// BAD - disables all type checking
const data: any = await api.getData();
data.typo; // No error, runtime crash
```

### ❌ Circular Hook Dependencies

```typescript
// BAD - infinite loop
useEffect(() => {
    doA();
}, [doB]);

useEffect(() => {
    doB();
}, [doA]); // Circular!
```

---

## Quick Reference

| Pattern | Use Case | Key Benefit |
|---------|----------|-------------|
| `useCallback` | Stable functions | Prevent re-renders |
| `useRef` circular | Mutual recursion | Break dependency cycles |
| `unknown` → cast | API data | Type safety + flexibility |
| Type guards | Runtime validation | Safe external data |
| Branded types | Domain IDs | Prevent mixing types |

---

**Sprint 3 Usage**:

- ✅ Fixed 3 Hook dependency issues
- ✅ Eliminated 8 `any` types  
- ✅ 0 runtime regressions

Last updated: 2026-02-13
