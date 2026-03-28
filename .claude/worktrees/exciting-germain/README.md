# Anykernel OS

> A bare-metal x86_64 operating system kernel built from scratch in C and Assembly.

## Stats

| Metric | Value |
|--------|-------|
| Version | v0.4.4 |
| Code | ~7,000 lines (C + NASM) |
| Kernel size | 76 KB |
| Boot time | 90 ms |
| Tests | 35 kernel + 5 runtime |
| Warnings | 0 (`-Werror`) |

## Architecture

```
Boot (Limine) → GDT/TSS → IDT (7 exceptions + 2 IRQs) → PIC/PIT
     → PMM (buddy) → VMM (4-level paging) → kmalloc (slab)
          → Scheduler (preemptive, 3-level priority)
               → Sync (spinlock, mutex, semaphore, waitqueue)
                    → IPC (message queue)
```

## Subsystems

### Boot & CPU (Sprint 0-1)

- **UART** — COM1 at 115200 baud (first hardware initialized)
- **GDT** — 7 entries + TSS with RSP0 and IST1 stacks
- **IDT** — #DE, #UD, #NM, #DF, #GP, #PF, #MF + IRQ0 (timer) + IRQ1 (keyboard)
- **SSP** — Stack Smashing Protector with RDTSC-randomized canary
- **CPUID** — Feature detection (SSE, NX, Long Mode, etc.)

### Memory (Sprint 2)

- **PMM** — Buddy allocator (orders 0-9, max 2MB blocks), peak tracking
- **VMM** — 4-level paging (PML4), per-section permissions (RX/R/RW+NX), guard pages
- **kmalloc** — Slab allocator (32B/64B/128B/256B + large), poisoning on free

### Hardware (Sprint 3)

- **PIC 8259** — Remapped to vectors 0x20+
- **PIT** — 100 Hz timer, tick counting, callbacks
- **Framebuffer Console** — 80×25, ANSI colors, scroll
- **PS/2 Keyboard** — Scancode set 1, shift, caps lock, ring buffer
- **klog** — 4KB ring buffer for post-mortem diagnostics

### Scheduler & Concurrency (Sprint 4)

- **Scheduler** — Preemptive, 3-level priority (HIGH > NORMAL > LOW)
- **Context Switch** — 12-instruction ASM routine (~20ns on hardware)
- **Tasks** — TCB with stack canary, CPU tick accounting, 64-task pool
- **Sleep/Join** — `task_sleep(ms)`, `task_join(tid)` with sleeping wait
- **Spinlock** — IRQ-safe with save/restore
- **Mutex** — Sleeping lock with owner tracking, trylock
- **Semaphore** — Counting semaphore (sem_wait/post/trywait)
- **Wait Queue** — Sleep/wake primitive (wq_wait/wake_one/wake_all)
- **Message Queue** — 64B messages, 16 capacity, blocking send/recv
- **ksnprintf** — Buffer-based printf formatting

### Trap Handling (Critical Fixes)

1. **EOI before schedule** — PIC never freezes
2. **sched_lock released in trampoline** — No deadlocks on first run
3. **sti in trampoline** — Interrupts always re-enabled for new tasks
4. **Deferred stack free** — Idle reaper frees stacks safely

## Test Results (v0.4.4)

### Kernel Self-Tests (35)

```
Spinlock, memops, PMM alloc/free/buddy/exhaust, kmalloc slab/large,
VMM map/unmap/guard, PIT ticks, keyboard buffer, console,
strncmp, strncpy, PMM peak, memcmp, ksnprintf
```

### Runtime Tests (5)

```
[TEST 1] Stress:   10 threads × 10,000 = 100,000/100,000  ✅
[TEST 2] Benchmark: context switch cycles measured          ✅
[TEST 3] IPC:      500 messages sent/received               ✅
[TEST 4] Timer:    sleep(200ms) = 200 ms (exact)            ✅
[TEST 5] Memory:   0 pages leaked                           ✅
```

## Building

```bash
# Requires: clang, nasm, xorriso, GNU make (MSYS2 recommended on Windows)
make clean && make iso
```

## Running

```bash
qemu-system-x86_64 -cdrom build/os.iso -serial stdio -m 128M
```

## File Structure

```
kernel/
├── main.c          Entry point + runtime tests
├── uart.c/h        Serial output
├── gdt.c/h         Global Descriptor Table + TSS
├── idt.c/h         Interrupt Descriptor Table
├── pmm.c/h         Physical Memory Manager (buddy)
├── vmm.c/h         Virtual Memory Manager (paging)
├── kmalloc.c/h     Kernel heap allocator (slab)
├── sched.c/h       Priority scheduler
├── task.h          TCB definition
├── context_switch.asm  ASM context switch
├── interrupts.asm  ISR/IRQ stubs
├── spinlock.h      IRQ-safe spinlocks
├── mutex.c/h       Sleeping locks
├── semaphore.c/h   Counting semaphores
├── waitqueue.c/h   Sleep/wake primitives
├── msgqueue.c/h    IPC message queue
├── pic.c/h         PIC + PIT driver
├── kb.c/h          PS/2 keyboard
├── console.c/h     Framebuffer console
├── kprintf.c/h     Kernel printf + snprintf
├── string.c/h      memset, memcpy, strcmp, etc.
├── klog.c/h        Ring buffer logger
├── cpuid.c/h       CPU feature detection
├── tests.c         35 self-tests
├── selftest.c/h    Test framework
├── panic.c/h       PANIC + KASSERT
├── ssp.c           Stack protector
├── log.h           LOG_OK / LOG_INFO / LOG_ERROR macros
├── memory.h        PAGE_SIZE, PHYS2VIRT macros
├── io.h            inb/outb port I/O
└── list.h          Intrusive linked list
```

## License

Educational / personal use.
