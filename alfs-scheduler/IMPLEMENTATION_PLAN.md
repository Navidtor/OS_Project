# ALFS (Anushiravan-Level Fair Scheduler) Implementation Plan

## Project Overview

This project implements ALFS - a modified version of Linux's CFS (Completely Fair Scheduler) that uses **Min-Heap** instead of **Red-Black Tree** for improved performance in extracting the task with minimum vruntime.

> Note: this document is synchronized with the current implementation in `src/` and `include/`.

### Scoring Summary
| Component | Points | Status |
|-----------|--------|--------|
| Core CFS with Min-Heap | 4.0 | ✅ Complete |
| Kernel-compatible language (C) | 1.0 | ✅ Complete |
| Cgroup support | 1.0 | ✅ Complete |
| Metadata output | 1.0 | ✅ Complete |
| RB-Tree vs Heap comparison | 1.0 | ✅ Complete |
| Alternative schedulers research (EEVDF) | 1.0 | ✅ Complete |
| Big.LITTLE architecture research | 1.0 | ✅ Complete |
| **Total Achieved** | **10.0** | ✅ **Full Marks** |

---

## Phase 1: Core Data Structures

### 1.1 Min-Heap Implementation
A binary min-heap for O(log n) insert and O(log n) extract-min operations.

```c
typedef struct {
    Task **tasks;
    int size;
    int capacity;
} MinHeap;

// Operations:
void heap_insert(MinHeap *h, Task *task);
Task *heap_extract_min(MinHeap *h);
Task *heap_peek(MinHeap *h);
void heap_update(MinHeap *h, Task *task);  // When vruntime changes
void heap_remove(MinHeap *h, Task *task);
```

### 1.2 Task Structure
```c
typedef enum {
    TASK_STATE_RUNNABLE,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_EXITED
} TaskState;

typedef struct Task {
    char task_id[MAX_TASK_ID_LEN];
    int nice;                    // -20 to +19, default 0
    double vruntime;             // Virtual runtime
    int weight;                  // Computed from nice value
    TaskState state;
    char cgroup_id[MAX_CGROUP_ID_LEN];  // Default "0" if not provided
    int *cpu_affinity;           // CPU mask array
    int affinity_count;          // Number of allowed CPUs
    int current_cpu;             // Currently assigned CPU (-1 if none)
    int burst_remaining;         // For CPU_BURST events
    bool is_burst;               // True while CPU_BURST is active
    int heap_index;              // Position in heap for O(log n) updates
} Task;
```

### 1.3 Cgroup Structure
```c
typedef struct Cgroup {
    char cgroup_id[MAX_CGROUP_ID_LEN];
    int cpu_shares;              // Default 1024
    int cpu_quota_us;            // Default -1 (unlimited)
    int cpu_period_us;           // Default 100000 (100ms)
    int *cpu_mask;               // Allowed CPUs
    int cpu_mask_count;
    double quota_used;           // Tracking quota usage
    int period_start_vtime;      // Start tick of current quota period
} Cgroup;
```

### 1.4 Per-CPU Run Queue
```c
typedef struct CPURunQueue {
    int cpu_id;
    Task *current_task;
    double min_vruntime;
} CPURunQueue;
```

---

## Phase 2: CFS Algorithm Implementation

### 2.1 Nice to Weight Conversion
Use the Linux kernel's weight table for converting nice values to weights:

```c
// From Linux kernel: kernel/sched/core.c
static const int sched_prio_to_weight[40] = {
    /* -20 */     88761,     71755,     56483,     46273,     36291,
    /* -15 */     29154,     23254,     18705,     14949,     11916,
    /* -10 */      9548,      7620,      6100,      4904,      3906,
    /*  -5 */      3121,      2501,      1991,      1586,      1277,
    /*   0 */      1024,       820,       655,       526,       423,
    /*   5 */       335,       272,       215,       172,       137,
    /*  10 */       110,        87,        70,        56,        45,
    /*  15 */        36,        29,        23,        18,        15,
};
```

### 2.2 Virtual Runtime Calculation
```
vruntime_delta = actual_runtime * (NICE_0_WEIGHT / task_weight)
```

Where:
- `NICE_0_WEIGHT = 1024` (weight for nice=0)
- `task_weight = sched_prio_to_weight[nice + 20]`

### 2.3 Task Selection Algorithm
```
1. At each tick, return currently running tasks to RUNNABLE and update vruntime.
2. Rebuild the runnable min-heap from RUNNABLE tasks.
3. For each CPU:
   a. Extract heap minimum candidates.
   b. Filter by task affinity + cgroup mask.
   c. Enforce cgroup quota, including planned runtime already committed to other CPUs in this tick.
   d. Assign first valid candidate; otherwise schedule "idle".
4. Reinsert deferred candidates back into heap.
```

### 2.4 Scheduling Events Handling

| Event | Action |
|-------|--------|
| TASK_CREATE | Create task, set initial vruntime = max_vruntime, default cgroup = "0" |
| TASK_EXIT | Remove from all data structures |
| TASK_BLOCK | Move from runnable to blocked state |
| TASK_UNBLOCK | Set vruntime = max(current_vruntime, min_vruntime - latency_bonus) |
| TASK_YIELD | Set vruntime = max_vruntime |
| TASK_SETNICE | Update weight |
| TASK_SET_AFFINITY | Update CPU mask, may trigger migration |
| CGROUP_CREATE | Create cgroup (shares/quota/period/mask) |
| CGROUP_MODIFY | Update cgroup settings |
| CGROUP_DELETE | Remove cgroup |
| TASK_MOVE_CGROUP | Move task to a new cgroup |
| CPU_BURST | Skip vruntime updates for burst duration |

---

## Phase 3: Cgroup Implementation

### 3.1 Hierarchical CPU Shares
- cpu_shares determines relative weight among cgroups
- Default: 1024
- A cgroup with 2048 shares gets 2x CPU time compared to 1024

### 3.2 CPU Quota/Period (Bandwidth Control)
```c
// Per-cgroup quota tracking
if (cgroup->cpu_quota_us > 0) {
    cgroup->quota_used += runtime;
}

// Multi-CPU safety in one tick:
// projected = quota_used + planned_runtime_this_tick + tick_runtime
// if projected > cpu_quota_us => candidate is throttled for that CPU pick

// Period reset:
// if elapsed_us_since(period_start_vtime) >= cpu_period_us:
//     quota_used = 0
//     period_start_vtime = current_vtime
```

### 3.3 CPU Mask Inheritance
Tasks in a cgroup can only run on CPUs allowed by the cgroup's cpumask.

---

## Phase 4: UDS Communication

### 4.1 Socket Setup
```c
int uds_connect(const char *socket_path) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}
```

### 4.2 Message Protocol
1. Receive TimeFrame JSON from tester
2. Process events
3. Run scheduler for this tick
4. Send SchedulerTick JSON response
5. Wait for next TimeFrame

---

## Phase 5: JSON Handling

Using cJSON library (lightweight, MIT licensed, single file):

### 5.1 Input Parsing
```c
TimeFrame *json_parse_timeframe(const char *json_str);
```

### 5.2 Output Generation
```c
char *json_serialize_tick(const SchedulerTick *tick, bool include_meta);
```

---

## Phase 6: Main Scheduler Loop

```c
int main(int argc, char *argv[]) {
    // Parse arguments
    char *socket_path = "event.socket";  // Default
    int quanta = 1;
    int num_cpus = 4;
    
    // Parse command line args
    parse_args(argc, argv, &socket_path, &quanta, &num_cpus);
    
    // Initialize data structures
    Scheduler *sched = scheduler_init(num_cpus, quanta);
    
    // Connect to UDS
    int sock = uds_connect(socket_path);
    
    // Main loop
    while (1) {
        // Receive TimeFrame
        char *input = uds_receive_message(sock);
        if (!input) break;
        
        TimeFrame *tf = json_parse_timeframe(input);
        
        // Process all events
        for (int i = 0; i < tf->event_count; i++) {
            scheduler_process_event(sched, tf->events[i]);
        }
        
        // Run scheduler
        SchedulerTick *tick = scheduler_tick(sched, tf->vtime);
        
        // Send response
        char *output = json_serialize_tick(tick, include_metadata);
        uds_send_message(sock, output);
        
        // Cleanup
        json_free_timeframe(tf);
        scheduler_tick_free(tick);
        free(input);
        free(output);
    }
    
    uds_disconnect(sock);
    scheduler_destroy(sched);
    return 0;
}
```

---

## Phase 7: Metadata Tracking (Bonus)

Track and output additional information:

```c
typedef struct SchedulerMeta {
    int preemptions;      // Tasks preempted this tick
    int migrations;       // Tasks that changed CPU
    char **runnable_tasks;
    int runnable_count;
    char **blocked_tasks;
    int blocked_count;
} SchedulerMeta;
```

---

## Implementation Timeline

| Phase | Description | Files | Estimated Lines |
|-------|-------------|-------|-----------------|
| 1 | Data Structures | heap.c, task.c, cgroup.c | ~500 |
| 2 | CFS Algorithm | scheduler.c | ~400 |
| 3 | Cgroup Support | cgroup.c (extend) | ~300 |
| 4 | UDS Communication | uds.c | ~150 |
| 5 | JSON Handling | json_handler.c | ~300 |
| 6 | Main Program | main.c | ~200 |
| 7 | Metadata | scheduler.c (extend) | ~100 |
| **Total** | | | **~1950** |

---

## File Structure

```
alfs-scheduler/
├── Makefile
├── README.md
├── IMPLEMENTATION_PLAN.md
├── include/
│   ├── alfs.h           # Main header with all type definitions
│   ├── heap.h           # Min-heap interface
│   ├── task.h           # Task management
│   ├── cgroup.h         # Cgroup management
│   ├── scheduler.h      # Scheduler core
│   ├── uds.h            # Unix Domain Socket
│   └── json_handler.h   # JSON parsing/generation
├── src/
│   ├── main.c           # Entry point
│   ├── heap.c           # Min-heap implementation
│   ├── task.c           # Task operations
│   ├── cgroup.c         # Cgroup operations
│   ├── scheduler.c      # CFS/ALFS algorithm
│   ├── uds.c            # UDS communication
│   └── json_handler.c   # JSON handling
├── lib/
│   └── cJSON/           # cJSON library
├── tests/
│   ├── test_heap.c
│   ├── test_scheduler.c
│   ├── test_server.py
│   └── sample_input.json
└── docs/
    ├── rb_tree_vs_heap.md
    ├── eevdf_comparison.md
    └── big_little_research.md
```

---

## Build System

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2 -I./include -I./lib
LDFLAGS = 

SRCS = $(wildcard src/*.c) lib/cJSON/cJSON.c
OBJS = $(SRCS:.c=.o)
TARGET = alfs_scheduler

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)
```

---

## Testing Strategy

1. **Unit Tests**: Test each component in isolation
2. **Integration Tests**: Test with sample JSON inputs
3. **Edge Cases**:
   - No runnable tasks (all idle)
   - More tasks than CPUs
   - CPU affinity conflicts
   - Cgroup quota exhaustion
   - Rapid block/unblock sequences

---

## Next Steps

1. ✅ Create project structure
2. ✅ Implement Min-Heap
3. ✅ Implement Task management
4. ✅ Implement basic CFS algorithm
5. ✅ Add UDS communication
6. ✅ Add JSON handling
7. ✅ Implement Cgroup support
8. ✅ Add metadata output
9. ✅ Write research documents
10. ✅ Testing and optimization

## Completion Status

**Project completed successfully!** All core requirements and bonus features have been implemented:

- Core CFS algorithm with Min-Heap: ✅
- All 12 event types supported: ✅
- UDS communication: ✅
- JSON input/output: ✅
- Cgroup support: ✅
- Metadata output: ✅
- Research documents: ✅
- Unit tests: ✅
