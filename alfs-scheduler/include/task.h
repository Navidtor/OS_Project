/**
 * ALFS - Task Management Interface
 */

#ifndef TASK_H
#define TASK_H

#include "alfs.h"

/**
 * Create a new task
 * @param task_id Task identifier
 * @param nice Nice value (-20 to +19)
 * @param cgroup_id Initial cgroup ID (NULL for default)
 * @return Pointer to new Task or NULL on failure
 */
Task *task_create(const char *task_id, int nice, const char *cgroup_id);

/**
 * Destroy a task and free memory
 * @param task Task to destroy
 */
void task_destroy(Task *task);

/**
 * Set task's CPU affinity
 * @param task Target task
 * @param cpu_mask Array of allowed CPU IDs
 * @param count Number of CPUs in mask
 * @return 0 on success, -1 on failure
 */
int task_set_affinity(Task *task, const int *cpu_mask, int count);

/**
 * Set task's nice value and update weight
 * @param task Target task
 * @param nice New nice value
 */
void task_set_nice(Task *task, int nice);

/**
 * Check if task can run on a specific CPU
 * @param task Task to check
 * @param cpu_id CPU ID to check
 * @return true if task can run on CPU, false otherwise
 */
bool task_can_run_on_cpu(const Task *task, int cpu_id);

/**
 * Update task's vruntime based on runtime
 * @param task Task to update
 * @param runtime Actual runtime (in quanta)
 */
void task_update_vruntime(Task *task, double runtime);

/**
 * Set task state
 * @param task Target task
 * @param state New state
 */
void task_set_state(Task *task, TaskState state);

/**
 * Copy task ID to a string
 * @param task Source task
 * @return Newly allocated string with task ID (caller must free)
 */
char *task_get_id(const Task *task);

#endif /* TASK_H */
