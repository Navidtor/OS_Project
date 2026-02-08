/**
 * ALFS - Scheduler Interface
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "alfs.h"

/**
 * Initialize the scheduler
 * @param cpu_count Number of CPUs
 * @param quanta Time quantum
 * @return Pointer to initialized Scheduler or NULL on failure
 */
Scheduler *scheduler_init(int cpu_count, int quanta);

/**
 * Destroy the scheduler and free all resources
 * @param sched Scheduler to destroy
 */
void scheduler_destroy(Scheduler *sched);

/**
 * Process an event
 * @param sched Scheduler
 * @param event Event to process
 * @return 0 on success, -1 on failure
 */
int scheduler_process_event(Scheduler *sched, const Event *event);

/**
 * Run the scheduler for one tick
 * @param sched Scheduler
 * @param vtime Current virtual time
 * @return SchedulerTick with schedule decisions
 */
SchedulerTick *scheduler_tick(Scheduler *sched, int vtime);

/**
 * Free a scheduler tick
 * @param tick Tick to free
 */
void scheduler_tick_free(SchedulerTick *tick);

/**
 * Find a task by ID
 * @param sched Scheduler
 * @param task_id Task ID to find
 * @return Task if found, NULL otherwise
 */
Task *scheduler_find_task(Scheduler *sched, const char *task_id);

/**
 * Find a cgroup by ID
 * @param sched Scheduler
 * @param cgroup_id Cgroup ID to find
 * @return Cgroup if found, NULL otherwise
 */
Cgroup *scheduler_find_cgroup(Scheduler *sched, const char *cgroup_id);

/**
 * Add a task to the scheduler
 * @param sched Scheduler
 * @param task Task to add
 * @return 0 on success, -1 on failure
 */
int scheduler_add_task(Scheduler *sched, Task *task);

/**
 * Remove a task from the scheduler
 * @param sched Scheduler
 * @param task_id Task ID to remove
 * @return 0 on success, -1 if not found
 */
int scheduler_remove_task(Scheduler *sched, const char *task_id);

/**
 * Add a cgroup to the scheduler
 * @param sched Scheduler
 * @param cgroup Cgroup to add
 * @return 0 on success, -1 on failure
 */
int scheduler_add_cgroup(Scheduler *sched, Cgroup *cgroup);

/**
 * Remove a cgroup from the scheduler
 * @param sched Scheduler
 * @param cgroup_id Cgroup ID to remove
 * @return 0 on success, -1 if not found
 */
int scheduler_remove_cgroup(Scheduler *sched, const char *cgroup_id);

/**
 * Get the minimum vruntime across all CPUs
 * @param sched Scheduler
 * @return Minimum vruntime
 */
double scheduler_get_min_vruntime(Scheduler *sched);

/**
 * Get the maximum vruntime across all runnable tasks
 * @param sched Scheduler
 * @return Maximum vruntime
 */
double scheduler_get_max_vruntime(Scheduler *sched);

#endif /* SCHEDULER_H */
