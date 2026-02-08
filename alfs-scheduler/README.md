# ALFS - Anushiravan-Level Fair Scheduler

A modified CFS (Completely Fair Scheduler) implementation that uses **Min-Heap** instead of **Red-Black Tree** for improved performance in task scheduling.

---

## What is This Project?

ALFS (Anushiravan-Level Fair Scheduler) is a **process scheduler** - a program that decides which tasks (programs) should run on your computer's CPU and when.

Think of it like a restaurant host who decides which customers get seated at which tables and in what order!

### What does it do?
- Receives "events" (like "create a new task" or "this task is waiting")
- Decides which tasks should run on each CPU
- Sends back the scheduling decisions via Unix Domain Sockets

### How is it different from Linux CFS?
- Uses **Min-Heap** instead of Red-Black Tree for task selection
- Simpler implementation with better cache performance
- Same virtual runtime algorithm as Linux kernel

---

## Features

### Core Features (Mandatory) - 4.0 Points ✅
- ✅ CFS-based scheduling with virtual runtime
- ✅ Nice value support (-20 to +19) with Linux kernel weight table
- ✅ Multi-CPU scheduling with per-CPU selection
- ✅ CPU affinity support
- ✅ Unix Domain Socket (UDS) communication
- ✅ JSON input/output format
- ✅ All 12 event types implemented

### Bonus Features - 6.0 Points ✅
- ✅ Written in C (kernel-compatible language) - 1.0 pt
- ✅ Cgroup support with CPU shares and quotas - 1.0 pt
- ✅ Metadata output (preemptions, migrations, task lists) - 1.0 pt
- ✅ RB-Tree vs Heap academic comparison - 1.0 pt
- ✅ EEVDF scheduler research (2+ papers + kernel references) - 1.0 pt
- ✅ Big.LITTLE architecture research (2+ papers + architecture references) - 1.0 pt
- ✅ CPU burst handling

### Total Score: 10.0/10.0 ✅

---

## Quick Start

```bash
# 1. Install dependencies (Ubuntu/Debian/WSL)
sudo apt update && sudo apt install -y build-essential python3

# 2. Build the project
make

# 3. Run tests
make test

# 4. Run the demo
./run_test.sh
```

---

## Getting Started

### Prerequisites

- **GCC** (GNU Compiler Collection)
- **Make** (GNU Make)
- **Python 3.6+** (for running the test server)

### Detailed Setup: Linux (Ubuntu/Debian)

Use this path if you are already on Linux.

#### 1. Install required packages
`build-essential` installs the C compiler and core build tools. `python3` is used by the local test server.

```bash
sudo apt update
sudo apt install -y build-essential gcc make python3
```

#### 2. Verify your toolchain
This confirms your environment is ready before building.

```bash
gcc --version
make --version
python3 --version
```

#### 3. Open the project directory

```bash
cd /path/to/alfs-scheduler
```

#### 4. Build from a clean state
`make clean` removes old objects so you do not debug stale binaries.

```bash
make clean
make
```

#### 5. Run all unit tests

```bash
make test
```

#### 6. Run full end-to-end demo
This starts the Python test server and scheduler together and validates socket communication.

```bash
chmod +x run_test.sh
./run_test.sh
```

#### 7. Manual run (two terminals, useful for debugging)

Terminal 1:
```bash
python3 tests/test_server.py event.socket tests/sample_input.json
```

Terminal 2:
```bash
./alfs_scheduler -s event.socket -c 4 -m
```

`event.socket` is a Unix Domain Socket file in your current directory. If a previous run crashes and leaves it behind, remove it with `rm -f event.socket`.

### Detailed Setup: Windows 10/11 with WSL2 + Ubuntu (Recommended)

Use this path on Windows. It gives a real Linux runtime, which this project expects for Unix Domain Sockets.

#### 1. Install WSL2 and Ubuntu
Open PowerShell as Administrator:

```powershell
wsl --install
```

Restart when prompted. On first launch, Ubuntu asks you to create a Linux username and password.

#### 2. Update Ubuntu packages

```bash
sudo apt update
sudo apt upgrade -y
```

#### 3. Install build dependencies inside Ubuntu

```bash
sudo apt install -y build-essential gcc make python3
```

#### 4. Put the project in the right place
WSL can access Windows files under `/mnt/c/...`, but builds are faster and more reliable when the repo is inside Linux home (`~/`).

Option A (work directly from Windows drive):
```bash
cd /mnt/c/Users/YOUR_WINDOWS_USERNAME/path/to/alfs-scheduler
```

Option B (recommended, copy into Linux filesystem):
```bash
cp -r /mnt/c/Users/YOUR_WINDOWS_USERNAME/path/to/alfs-scheduler ~/alfs-scheduler
cd ~/alfs-scheduler
```

#### 5. Build and test

```bash
make clean
make
make test
```

#### 6. Run full integration demo

```bash
chmod +x run_test.sh
./run_test.sh
```

#### 7. Optional: run from VS Code with WSL extension
Install the VS Code `WSL` extension, then use `WSL: Connect to WSL` and open the folder inside Ubuntu. This keeps terminal, compiler, and socket behavior consistent with Linux.

#### 8. Windows/WSL tips
- If scripts fail with line-ending errors, convert to LF:
```bash
sed -i 's/\r$//' run_test.sh
chmod +x run_test.sh
```
- If socket bind/connect fails:
```bash
rm -f event.socket
```
- If WSL command fails with virtualization errors (for example `0x80370102`), enable virtualization in BIOS/UEFI and ensure required Windows virtualization features are on.

---

## Building

| Command | Description |
|---------|-------------|
| `make` | Build the project |
| `make clean` | Remove all build files |
| `make debug` | Build with debug symbols and sanitizers |
| `make test` | Build and run all tests |
| `make test_heap` | Run only heap tests |
| `make test_scheduler` | Run only scheduler tests |

### Understanding Compiler Flags

```
gcc -Wall -Wextra -Werror -pedantic -std=c11 -O2
```

| Flag | Meaning |
|------|---------|
| `-Wall` | Enable all common warnings |
| `-Wextra` | Enable extra warnings |
| `-Werror` | Treat warnings as errors |
| `-pedantic` | Strict ISO C compliance |
| `-std=c11` | Use C11 standard |
| `-O2` | Optimization level 2 |

---

## Running the Project

### Method 1: Quick Start (Recommended)

```bash
chmod +x run_test.sh   # Make executable (first time only)
./run_test.sh          # Run the demo
```

### Method 2: Manual Execution (Two Terminals)

**Terminal 1 - Start the test server:**
```bash
python3 tests/test_server.py event.socket tests/sample_input.json
```

**Terminal 2 - Start the scheduler:**
```bash
./alfs_scheduler -s event.socket -c 4 -m
```

### Method 3: VS Code (Two Terminals)

1. Open terminal: `` Ctrl + ` ``
2. Click `+` to create second terminal
3. Run test server in Terminal 1
4. Run scheduler in Terminal 2

### Expected Output

```
========================================
ALFS Test Runner
========================================
Socket: event.socket
Input:  tests/sample_input.json
CPUs:   4
========================================
Starting test server...
Starting ALFS scheduler...
ALFS Scheduler Starting...
  Socket: event.socket
  CPUs: 4
  Quanta: 1
  Metadata: enabled
Connecting to socket: event.socket
Connected. Waiting for events...

Sent vtime=0: 3 events
  Response vtime=0: [idle, idle, idle, idle]
Sent vtime=1: 2 events
  Response vtime=1: [T_build_1, T_build_2, idle, idle]
...
============================================================
TEST COMPLETE
============================================================
Total timeframes: 20
Responses received: 20
Results written to: tests/sample_input_output.json
```

---

## Command Line Options

```bash
./alfs_scheduler [options]
```

| Short | Long | Description | Default |
|-------|------|-------------|---------|
| `-s` | `--socket` | Socket file path | `event.socket` |
| `-c` | `--cpus` | Number of CPUs | `4` |
| `-q` | `--quanta` | Time quantum | `1` |
| `-m` | `--metadata` | Include metadata in output | off |
| `-h` | `--help` | Show help message | - |

### Examples

```bash
./alfs_scheduler                        # Default settings
./alfs_scheduler -c 8 -m                # 8 CPUs with metadata
./alfs_scheduler -s /tmp/sched.socket   # Custom socket path
./alfs_scheduler --help                 # Show help
```

---

## Communication Protocol

### Input Format (TimeFrame)

```json
{
  "vtime": 0,
  "events": [
    {
      "action": "TASK_CREATE",
      "taskId": "T1",
      "nice": 0,
      "cgroupId": "0"
    }
  ]
}
```

If `cgroupId` is omitted, the scheduler uses `"0"` as the default main cgroup.

### Output Format (SchedulerTick)

```json
{
  "vtime": 0,
  "schedule": ["T1", "idle", "idle", "idle"],
  "meta": {
    "preemptions": 0,
    "migrations": 0,
    "runnableTasks": ["T1"],
    "blockedTasks": []
  }
}
```

### Understanding the Output

| Field | Meaning |
|-------|---------|
| `vtime` | Virtual time (clock tick) |
| `schedule` | Task assigned to each CPU (`idle` if none) |
| `preemptions` | Tasks stopped to run another task |
| `migrations` | Tasks that moved to a different CPU |
| `runnableTasks` | Tasks ready to run |
| `blockedTasks` | Tasks waiting (I/O, sleep, etc.) |

---

## Supported Events

| Event | Description |
|-------|-------------|
| `TASK_CREATE` | Create a new task |
| `TASK_EXIT` | Terminate a task |
| `TASK_BLOCK` | Block a task (I/O wait, sleep, etc.) |
| `TASK_UNBLOCK` | Unblock a previously blocked task |
| `TASK_YIELD` | Voluntarily yield CPU |
| `TASK_SETNICE` | Change task nice value (-20 to +19) |
| `TASK_SET_AFFINITY` | Set CPU affinity mask |
| `CGROUP_CREATE` | Create a new cgroup |
| `CGROUP_MODIFY` | Modify cgroup parameters |
| `CGROUP_DELETE` | Delete a cgroup |
| `TASK_MOVE_CGROUP` | Move task to different cgroup |
| `CPU_BURST` | Mark task as CPU-intensive |

---

## Algorithm Details

### Virtual Runtime Calculation

```
vruntime_delta = actual_runtime × (NICE_0_WEIGHT / task_weight)
```

Where:
- `NICE_0_WEIGHT = 1024` (weight for nice=0)
- `task_weight` comes from the Linux kernel's `sched_prio_to_weight` table

### Task Selection

For each CPU, select the runnable task with:
1. Minimum vruntime
2. Compatible CPU affinity
3. Compatible cgroup CPU mask
4. Available cgroup quota

### Special Cases

| Scenario | Handling |
|----------|----------|
| New task | `vruntime = max_vruntime` (prevents starvation) |
| Unblocked task | Small priority boost for I/O-bound tasks |
| Yielded task | `vruntime = max_vruntime` (lets others run) |
| CPU burst | vruntime not updated during burst |

---

## Project Structure

```
alfs-scheduler/
├── Makefile              # Build configuration
├── README.md             # This file
├── IMPLEMENTATION_PLAN.md
├── run_test.sh           # Easy test runner
├── include/              # Header files
│   ├── alfs.h            # Main definitions & constants
│   ├── heap.h            # Min-heap interface
│   ├── scheduler.h       # Scheduler core
│   ├── task.h            # Task management
│   ├── cgroup.h          # Cgroup management
│   ├── uds.h             # Unix Domain Socket
│   └── json_handler.h    # JSON handling
├── src/                  # Source files
│   ├── main.c            # Entry point
│   ├── heap.c            # Min-heap implementation
│   ├── task.c            # Task operations
│   ├── cgroup.c          # Cgroup operations
│   ├── scheduler.c       # CFS/ALFS algorithm
│   ├── uds.c             # Socket communication
│   └── json_handler.c    # JSON parsing/generation
├── lib/
│   └── cJSON/            # JSON library (bundled)
├── tests/
│   ├── test_heap.c       # Heap unit tests
│   ├── test_scheduler.c  # Scheduler unit tests
│   ├── test_server.py    # Python test server
│   └── sample_input.json # Sample test input
└── docs/                 # Research documents
    ├── rb_tree_vs_heap.md
    ├── eevdf_comparison.md
    └── big_little_research.md
```

---

## Testing

### Unit Tests

```bash
make test  # Run all tests (22 total: 6 heap + 16 scheduler)
```

**Expected output:**
```
Running Min-Heap Tests...
  [PASS] test_heap_create_destroy
  [PASS] test_heap_insert_extract
  [PASS] test_heap_peek
  [PASS] test_heap_update
  [PASS] test_heap_remove
  [PASS] test_heap_stress

All heap tests passed!

Running Scheduler Tests...
  [PASS] test_scheduler_init
  [PASS] test_task_create_event
  [PASS] test_basic_scheduling
  [PASS] test_task_block_unblock
  [PASS] test_nice_values
  [PASS] test_cpu_affinity
  [PASS] test_cgroup_create
  [PASS] test_task_yield
  [PASS] test_task_exit
  [PASS] test_invalid_event_action
  [PASS] test_cgroup_quota_enforcement
  [PASS] test_cgroup_quota_multi_cpu_enforcement
  [PASS] test_cgroup_shares_effect
  [PASS] test_cgroup_modify_delete
  [PASS] test_task_move_cgroup
  [PASS] test_cpu_burst_vruntime

All scheduler tests passed!
```

### Debug Build

For debugging with AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
make debug
```

This enables:
- Debug symbols (`-g`)
- Address sanitizer (`-fsanitize=address`)
- Undefined behavior sanitizer (`-fsanitize=undefined`)
- No optimization (`-O0`)

---

## VS Code Integration

### Setup

1. Install **WSL** extension (Windows only)
2. Install **C/C++** extension by Microsoft
3. Connect to WSL: `Ctrl + Shift + P` → `WSL: Connect to WSL`
4. Open terminal: `` Ctrl + ` ``

### Recommended Extensions

| Extension | Purpose |
|-----------|---------|
| **C/C++** (Microsoft) | IntelliSense, debugging |
| **WSL** (Microsoft) | Linux on Windows |
| **Makefile Tools** (Microsoft) | Makefile support |
| **GitLens** | Git history |
| **Error Lens** | Inline error display |

### Useful Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `` Ctrl + ` `` | Toggle terminal |
| `` Ctrl + Shift + ` `` | New terminal |
| `Ctrl + Shift + P` | Command palette |
| `Ctrl + Shift + E` | File explorer |
| `Ctrl + Shift + F` | Search in files |
| `F12` | Go to definition |
| `Ctrl + /` | Toggle comment |

---

## Troubleshooting

### Common Problems

| Problem | Solution |
|---------|----------|
| `command not found: make` | `sudo apt install build-essential` |
| `command not found: gcc` | `sudo apt install gcc` |
| `command not found: python3` | `sudo apt install python3` |
| `Permission denied` | `chmod +x run_test.sh alfs_scheduler` |
| `Failed to connect to socket` | `rm -f event.socket` |
| `Address already in use` | `rm -f event.socket` |
| Build errors | `make clean && make` |
| Slow on WSL2 | Copy project to `~/` instead of `/mnt/c/` |

### WSL2-Specific Issues

| Problem | Solution |
|---------|----------|
| `wsl --install` fails | Run PowerShell as Administrator |
| Can't find files | Use `/mnt/c/Users/USERNAME/...` path |
| VS Code not connecting | Install WSL extension, restart VS Code |
| Error 0x80370102 | Enable virtualization in BIOS |
| Terminal shows PowerShell | Click dropdown → Select "Ubuntu (WSL)" |

### Line Ending Issues

If you get `cannot execute: required file not found`:

```bash
# Fix line endings
sed -i 's/\r$//' run_test.sh
chmod +x run_test.sh
```

Or in VS Code: Click `CRLF` in bottom-right → Select `LF`

---

## WSL Path Reference

| Windows | Linux (WSL) |
|---------|-------------|
| `C:\` | `/mnt/c/` |
| `D:\` | `/mnt/d/` |
| `C:\Users\John\` | `/mnt/c/Users/John/` |
| `C:\Users\John\Downloads` | `/mnt/c/Users/John/Downloads` |

---

## Glossary

| Term | Meaning |
|------|---------|
| **Scheduler** | Program that decides which tasks run when |
| **vruntime** | Virtual runtime - fair scheduling metric |
| **Nice value** | Priority (-20 highest to +19 lowest) |
| **Cgroup** | Control group for resource limits |
| **UDS** | Unix Domain Socket - local IPC |
| **Min-Heap** | Data structure for O(1) minimum access |
| **Preemption** | Stopping a task to run another |
| **Migration** | Moving a task to different CPU |
| **Affinity** | Which CPUs a task can run on |
| **WSL** | Windows Subsystem for Linux |

---

## Documentation

Research documents available in the `docs/` folder:

| Document | Description |
|----------|-------------|
| [`rb_tree_vs_heap.md`](docs/rb_tree_vs_heap.md) | Academic comparison of Red-Black Tree vs Min-Heap with literature references |
| [`eevdf_comparison.md`](docs/eevdf_comparison.md) | Analysis of EEVDF scheduler vs CFS/ALFS with papers + kernel references |
| [`big_little_research.md`](docs/big_little_research.md) | Big.LITTLE architecture scheduling research with papers + architecture references |

---

## Project Status

| Component | Status |
|-----------|--------|
| Build | ✅ Compiles with strict flags |
| Heap Tests | ✅ 6/6 passing |
| Scheduler Tests | ✅ 16/16 passing |
| Integration Test | ✅ 20/20 timeframes |
| Core Features | ✅ Complete |
| Bonus Features | ✅ Complete |

---

## Success Checklist

### All Users
- [ ] Installed build tools (gcc, make, python3)
- [ ] Built project successfully (`make`)
- [ ] All tests pass (`make test`)
- [ ] Demo runs (`./run_test.sh`)

### Windows Users
- [ ] WSL2 installed and working
- [ ] Ubuntu setup complete
- [ ] Can access project from WSL

### VS Code Users
- [ ] WSL extension installed (Windows)
- [ ] C/C++ extension installed
- [ ] Terminal works in VS Code

---

## License

This project is developed as an academic assignment for the Operating Systems course.

## Author

Operating Systems Course Project - ALFS Implementation
