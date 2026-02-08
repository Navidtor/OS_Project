# ALFS Socket Exam Guide

This guide is for the exam scenario where the examiner tests your scheduler through Unix Domain Sockets (UDS) using JSON events.

## 1. What the examiner is likely to do

1. Run (or ask you to run) a UDS server/tester.
2. Send JSON `TimeFrame` messages to your scheduler.
3. Receive one JSON `SchedulerTick` response per input frame.
4. Check behavior for task events, fairness, affinity, cgroups, and metadata.

Your scheduler is designed for this flow.

## 2. Pre-exam checklist (do this first)

```bash
cd /path/to/alfs-scheduler
make clean
make
make test
```

If all tests pass, you are in a safe state.

## 3. Fast exam workflow (two terminals)

Use this when examiner gives you a JSON file.

### Terminal 1 (server/tester)

```bash
rm -f event.socket
python3 tests/test_server.py event.socket /path/to/exam_input.json
```

### Terminal 2 (your scheduler)

```bash
./alfs_scheduler -s event.socket -c 4 -m
```

Notes:
- `-s` sets socket path.
- `-c` sets CPU count.
- `-m` includes metadata in output.

## 4. If examiner has their own tester

Ask them:
1. Socket path
2. CPU count they expect
3. Whether metadata is required

Then run only your scheduler:

```bash
./alfs_scheduler -s /their/socket/path -c <cpus> -m
```

Example:

```bash
./alfs_scheduler -s /tmp/exam.socket -c 8 -m
```

## 5. JSON examples you should understand

### Input `TimeFrame` (single frame over socket)

```json
{
  "vtime": 3,
  "events": [
    { "action": "TASK_CREATE", "taskId": "T1", "nice": 0, "cgroupId": "0" },
    { "action": "TASK_SET_AFFINITY", "taskId": "T1", "cpuMask": [0, 1] }
  ]
}
```

### Output `SchedulerTick`

```json
{
  "vtime": 3,
  "schedule": ["T1", "idle", "idle", "idle"],
  "meta": {
    "preemptions": 0,
    "migrations": 0,
    "runnableTasks": ["T1"],
    "blockedTasks": []
  }
}
```

## 6. File format vs socket format (important)

- Over socket: tester sends one `TimeFrame` object at a time.
- In `tests/test_server.py` input file: file is an array of timeframes.

Example file format:

```json
[
  {
    "vtime": 0,
    "events": [
      { "action": "TASK_CREATE", "taskId": "A", "nice": 0 }
    ]
  },
  {
    "vtime": 1,
    "events": []
  }
]
```

## 7. Actions you should be ready to explain

- `TASK_CREATE`
- `TASK_EXIT`
- `TASK_BLOCK`
- `TASK_UNBLOCK`
- `TASK_YIELD`
- `TASK_SETNICE`
- `TASK_SET_AFFINITY`
- `CGROUP_CREATE`
- `CGROUP_MODIFY`
- `CGROUP_DELETE`
- `TASK_MOVE_CGROUP`
- `CPU_BURST`

Also know:
- Unknown action is rejected (not silently treated as create).

## 8. Live troubleshooting during exam

### `Failed to connect to socket`

```bash
rm -f event.socket
```

Then restart server first, scheduler second.

### `Address already in use`

```bash
rm -f event.socket
```

### Script permission issue

```bash
chmod +x run_test.sh
```

### Line ending issue on WSL

```bash
sed -i 's/\r$//' run_test.sh
chmod +x run_test.sh
```

## 9. 60-second explanation script (oral exam)

1. "My scheduler uses CFS-style vruntime fairness with a Min-Heap for runnable tasks."
2. "Input arrives as JSON events over UDS; output is one schedule per vtime tick."
3. "Nice values map to Linux weight table; vruntime grows by `runtime * 1024 / weight`."
4. "I support affinity, cgroups (shares/quota/period/mask), migrations, and metadata."
5. "I validate unknown actions and handle cgroup modifications/deletions safely."

## 10. Last-minute rehearsal command sequence

```bash
make clean && make test
rm -f event.socket
python3 tests/test_server.py event.socket tests/sample_input.json
# in second terminal:
./alfs_scheduler -s event.socket -c 4 -m
```

If this run is clean, you are ready for a socket-based practical exam.

### Advanced rehearsal with comprehensive scenario

Use this when you want stronger stress coverage before exam:

```bash
rm -f event.socket
python3 tests/test_server.py event.socket tests/sample_input2.json
# in second terminal:
./alfs_scheduler -s event.socket -c 4 -m
```

Expected result:
- `Total timeframes: 25`
- `Responses received: 25`
- Output file: `tests/sample_input2_output.json`

## 11. How to read `tests/sample_input_output.json` in exam

This file is the full scheduler response trace for each input timeframe.

Each item in the JSON array is one scheduler tick result.

### 11.1 Meaning of each field

- `vtime`: current tick number.
- `schedule`: selected task for each CPU, in order:
  - index `0` = CPU0
  - index `1` = CPU1
  - index `2` = CPU2
  - index `3` = CPU3
- `meta.preemptions`: number of CPUs where running task changed from previous tick.
- `meta.migrations`: number of tasks moved from one CPU to another.
- `meta.runnableTasks`: tasks ready to run (including currently running).
- `meta.blockedTasks`: tasks blocked by `TASK_BLOCK` (or similar non-runnable state).

### 11.2 Quick interpretation rules

1. If all schedule entries are `"idle"`, no runnable task exists.
2. If `blockedTasks` grows, some task was blocked and should disappear from CPU assignment.
3. If `preemptions` is high, scheduling order changed significantly in that tick.
4. If `migrations` is high, tasks moved between CPUs (often after affinity/cgroup changes).
5. If tasks gradually disappear and CPUs become `idle`, exit events were processed correctly.

### 11.3 Your sample output explained (timeline)

Use this script in presentation/exam:

1. `vtime=0`: all `idle`.
   - Means only setup events (like cgroup creation) happened; no task is runnable yet.
2. `vtime=1..3`: task creation phase.
   - `T_build_1`, `T_build_2`, then `T_ui_1`, `T_daemon_1` appear in schedule/runnable list.
3. `vtime=4..6`: blocked-task phase.
   - `T_build_2` is in `blockedTasks`, and no longer scheduled.
4. `vtime=7`: unblock phase.
   - `T_build_2` returns to schedule and leaves `blockedTasks`.
5. `vtime=8`: major rebalancing tick.
   - `preemptions=3`, `migrations=3` indicates strong CPU assignment reshuffle.
6. `vtime=9`: new task arrives.
   - `T_build_3` appears in both `schedule` and `runnableTasks`.
7. `vtime=10..12`: stable scheduling window.
   - `preemptions=0`, `migrations=0`; assignments remain steady.
8. `vtime=13`: `T_ui_1` blocked.
   - It disappears from schedule and appears in `blockedTasks`.
9. `vtime=14`: `T_ui_1` unblocked.
   - Returns to `schedule`; preemption/migration rises again due to change.
10. `vtime=15..19`: exit/cleanup phase.
    - Tasks exit one-by-one; by `vtime=19` all CPUs are `idle`, runnable list empty.

### 11.4 One-line summary for examiner

"`sample_input_output.json` shows that for every input timeframe, ALFS produced one valid scheduling decision, tracked metadata, respected blocked/unblocked transitions, and ended cleanly when all tasks exited."

## 12. What a correct run looks like

### Terminal 1 (tester) success pattern

You should see:
- `Loaded N timeframes ...`
- `Scheduler connected!`
- repeated `Sent vtime=...` and `Response vtime=...`
- final summary with:
  - `Total timeframes: N`
  - `Responses received: N`
  - output file written

If `Responses received` is less than `Total timeframes`, the run is not fully correct.

### Terminal 2 (scheduler) success pattern

Normal successful ending is:

```text
Connected. Waiting for events...
Connection closed by peer

Shutting down...
ALFS Scheduler terminated.
```

`Connection closed by peer` is expected when tester finishes and closes the socket.

## 13. Event JSON cheat sheet (required fields)

Use this to answer examiner questions quickly.

| Event | Required fields | Common optional fields |
|------|------------------|------------------------|
| `TASK_CREATE` | `action`, `taskId` | `nice`, `cgroupId`, `cpuMask` |
| `TASK_EXIT` | `action`, `taskId` | - |
| `TASK_BLOCK` | `action`, `taskId` | - |
| `TASK_UNBLOCK` | `action`, `taskId` | - |
| `TASK_YIELD` | `action`, `taskId` | - |
| `TASK_SETNICE` | `action`, `taskId`, (`newNice` or `nice`) | - |
| `TASK_SET_AFFINITY` | `action`, `taskId`, `cpuMask` | - |
| `CGROUP_CREATE` | `action`, `cgroupId` | `cpuShares`, `cpuQuotaUs`, `cpuPeriodUs`, `cpuMask` |
| `CGROUP_MODIFY` | `action`, `cgroupId` | `cpuShares`, `cpuQuotaUs`, `cpuPeriodUs`, `cpuMask` |
| `CGROUP_DELETE` | `action`, `cgroupId` | - |
| `TASK_MOVE_CGROUP` | `action`, `taskId`, `newCgroupId` | - |
| `CPU_BURST` | `action`, `taskId`, `duration` | - |

Notes:
- Unknown `action` is rejected.
- `cpuQuotaUs: null` means unlimited quota.
- If `cgroupId` is omitted in `TASK_CREATE`, default cgroup `"0"` is used.

## 14. Fast fixes for common exam-time failures

### `./alfs_scheduler: No such file or directory`

Build the binary:

```bash
make
```

Then run again:

```bash
./alfs_scheduler -s event.socket -c 4 -m
```

### `command not found: make` or `gcc`

```bash
sudo apt update
sudo apt install -y build-essential gcc make python3
```

### JSON input has syntax issues

Validate first:

```bash
python3 -m json.tool /path/to/exam_input.json > /dev/null
```

## 15. High-probability examiner checks

Be ready to demonstrate these quickly:

1. **Block/Unblock behavior**
   - Show task enters `blockedTasks` then returns.
2. **Affinity restriction**
   - Set `cpuMask` and show task only runs on allowed CPUs.
3. **Nice effect**
   - Change `newNice` and explain weight/vruntime impact.
4. **Cgroup quota/period**
   - Show throttling (`idle` appears) then recovery after period reset.
5. **Task migration**
   - Move task cgroup or affinity and point to `migrations` metadata.
6. **Graceful end**
   - Explain why `Connection closed by peer` is normal at completion.

## 16. One-command coverage check before exam

Use this scenario to cover most checks in one run:

```bash
rm -f event.socket
python3 tests/test_server.py event.socket tests/sample_input2.json
# second terminal:
./alfs_scheduler -s event.socket -c 4 -m
```

Then confirm:
- `Responses received: 25`
- `tests/sample_input2_output.json` exists
