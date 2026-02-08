/**
 * ALFS - Scheduler Core Implementation
 * 
 * This implements the CFS (Completely Fair Scheduler) algorithm
 * using a Min-Heap instead of Red-Black Tree for O(log n) operations.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "scheduler.h"
#include "heap.h"
#include "task.h"
#include "cgroup.h"

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * Check if a task can run on a specific CPU considering both
 * task affinity and cgroup CPU mask
 */
static bool can_task_run_on_cpu(Scheduler *sched, Task *task, int cpu_id) {
    /* Check task affinity */
    if (!task_can_run_on_cpu(task, cpu_id)) {
        return false;
    }
    
    /* Check cgroup CPU mask */
    if (task->cgroup_id[0] != '\0') {
        Cgroup *cgroup = scheduler_find_cgroup(sched, task->cgroup_id);
        if (cgroup && !cgroup_allows_cpu(cgroup, cpu_id)) {
            return false;
        }
    }
    
    return true;
}

/**
 * Get the minimum vruntime across all runnable tasks
 */
static double get_min_vruntime(Scheduler *sched) {
    double min_vr = DBL_MAX;
    
    for (int i = 0; i < sched->task_count; i++) {
        Task *task = sched->all_tasks[i];
        if (task->state == TASK_STATE_RUNNABLE || task->state == TASK_STATE_RUNNING) {
            if (task->vruntime < min_vr) {
                min_vr = task->vruntime;
            }
        }
    }
    
    return min_vr == DBL_MAX ? 0.0 : min_vr;
}

/**
 * Get the maximum vruntime across all runnable tasks
 */
static double get_max_vruntime(Scheduler *sched) {
    double max_vr = 0.0;
    
    for (int i = 0; i < sched->task_count; i++) {
        Task *task = sched->all_tasks[i];
        if (task->state == TASK_STATE_RUNNABLE || task->state == TASK_STATE_RUNNING) {
            if (task->vruntime > max_vr) {
                max_vr = task->vruntime;
            }
        }
    }
    
    return max_vr;
}

/**
 * Get task weight adjusted by cgroup shares.
 */
static int get_effective_task_weight(Scheduler *sched, Task *task) {
    long long weight = task->weight;
    if (task->cgroup_id[0] != '\0') {
        Cgroup *cgroup = scheduler_find_cgroup(sched, task->cgroup_id);
        if (cgroup && cgroup->cpu_shares > 0) {
            weight = (weight * cgroup->cpu_shares) / DEFAULT_CPU_SHARES;
        }
    }
    if (weight < 1) {
        weight = 1;
    }
    return (int)weight;
}

/**
 * Rebuild runnable heap from current task states.
 */
static void rebuild_runnable_heap(Scheduler *sched) {
    sched->runnable_heap->size = 0;
    for (int i = 0; i < sched->task_count; i++) {
        Task *task = sched->all_tasks[i];
        task->heap_index = -1;
        if (task->state == TASK_STATE_RUNNABLE) {
            heap_insert(sched->runnable_heap, task);
        }
    }
}

/**
 * Reset cgroup periods when their accounting window expires.
 */
static void refresh_cgroup_periods(Scheduler *sched, int vtime) {
    long long tick_us = (long long)sched->quanta * 1000LL;
    if (tick_us <= 0) {
        tick_us = 1000LL;
    }
    
    for (int i = 0; i < sched->cgroup_count; i++) {
        Cgroup *cgroup = sched->cgroups[i];
        if (!cgroup || cgroup->cpu_period_us <= 0) {
            continue;
        }
        
        if (vtime < cgroup->period_start_vtime) {
            cgroup_reset_period(cgroup, vtime);
            continue;
        }
        
        long long elapsed_ticks = (long long)vtime - (long long)cgroup->period_start_vtime;
        long long elapsed_us = elapsed_ticks * tick_us;
        if (elapsed_us >= (long long)cgroup->cpu_period_us) {
            cgroup_reset_period(cgroup, vtime);
        }
    }
}

/**
 * Pick best runnable task for a CPU by repeatedly extracting heap minimum.
 * Non-eligible tasks are reinserted back into heap.
 */
static double get_planned_runtime_for_cgroup(Cgroup **planned_cgroups,
                                             double *planned_runtime_us,
                                             int planned_count,
                                             Cgroup *target) {
    for (int i = 0; i < planned_count; i++) {
        if (planned_cgroups[i] == target) {
            return planned_runtime_us[i];
        }
    }
    return 0.0;
}

static void add_planned_runtime_for_cgroup(Cgroup **planned_cgroups,
                                           double *planned_runtime_us,
                                           int *planned_count,
                                           Cgroup *target,
                                           double delta_us) {
    for (int i = 0; i < *planned_count; i++) {
        if (planned_cgroups[i] == target) {
            planned_runtime_us[i] += delta_us;
            return;
        }
    }
    if (*planned_count < MAX_CGROUPS) {
        planned_cgroups[*planned_count] = target;
        planned_runtime_us[*planned_count] = delta_us;
        (*planned_count)++;
    }
}

static Task *pick_task_for_cpu(Scheduler *sched,
                               int cpu,
                               Cgroup **planned_cgroups,
                               double *planned_runtime_us,
                               int *planned_count,
                               double tick_runtime_us) {
    Task *deferred[MAX_TASKS];
    int deferred_count = 0;
    Task *selected = NULL;
    
    while (!heap_is_empty(sched->runnable_heap)) {
        Task *candidate = heap_extract_min(sched->runnable_heap);
        if (!candidate) {
            break;
        }
        
        if (!can_task_run_on_cpu(sched, candidate, cpu)) {
            deferred[deferred_count++] = candidate;
            continue;
        }
        
        if (candidate->cgroup_id[0] != '\0') {
            Cgroup *cgroup = scheduler_find_cgroup(sched, candidate->cgroup_id);
            if (cgroup && !cgroup_has_quota(cgroup, sched->current_vtime)) {
                deferred[deferred_count++] = candidate;
                continue;
            }
            if (cgroup && cgroup->cpu_quota_us >= 0) {
                double planned = get_planned_runtime_for_cgroup(planned_cgroups,
                                                                planned_runtime_us,
                                                                *planned_count,
                                                                cgroup);
                double projected = cgroup->quota_used + planned + tick_runtime_us;
                if (projected > (double)cgroup->cpu_quota_us) {
                    deferred[deferred_count++] = candidate;
                    continue;
                }
            }
        }
        
        selected = candidate;
        break;
    }
    
    for (int i = 0; i < deferred_count; i++) {
        heap_insert(sched->runnable_heap, deferred[i]);
    }
    
    if (selected && selected->cgroup_id[0] != '\0') {
        Cgroup *cgroup = scheduler_find_cgroup(sched, selected->cgroup_id);
        if (cgroup && cgroup->cpu_quota_us >= 0) {
            add_planned_runtime_for_cgroup(planned_cgroups,
                                           planned_runtime_us,
                                           planned_count,
                                           cgroup,
                                           tick_runtime_us);
        }
    }
    
    return selected;
}

/* ============================================================================
 * Public Functions - Initialization
 * ============================================================================ */

Scheduler *scheduler_init(int cpu_count, int quanta) {
    Scheduler *sched = calloc(1, sizeof(Scheduler));
    if (!sched) {
        return NULL;
    }
    
    sched->cpu_count = cpu_count;
    sched->quanta = quanta > 0 ? quanta : 1;
    sched->current_vtime = 0;
    
    /* Initialize CPU queues */
    sched->cpu_queues = calloc(cpu_count, sizeof(CPURunQueue));
    if (!sched->cpu_queues) {
        free(sched);
        return NULL;
    }
    
    for (int i = 0; i < cpu_count; i++) {
        sched->cpu_queues[i].cpu_id = i;
        sched->cpu_queues[i].current_task = NULL;
        sched->cpu_queues[i].min_vruntime = 0.0;
    }
    
    /* Initialize task storage */
    sched->task_capacity = MAX_TASKS;
    sched->all_tasks = calloc(sched->task_capacity, sizeof(Task *));
    if (!sched->all_tasks) {
        free(sched->cpu_queues);
        free(sched);
        return NULL;
    }
    sched->task_count = 0;
    
    /* Initialize cgroup storage */
    sched->cgroup_capacity = MAX_CGROUPS;
    sched->cgroups = calloc(sched->cgroup_capacity, sizeof(Cgroup *));
    if (!sched->cgroups) {
        free(sched->all_tasks);
        free(sched->cpu_queues);
        free(sched);
        return NULL;
    }
    sched->cgroup_count = 0;
    
    /* Initialize runnable heap */
    sched->runnable_heap = heap_create(MAX_TASKS);
    if (!sched->runnable_heap) {
        free(sched->cgroups);
        free(sched->all_tasks);
        free(sched->cpu_queues);
        free(sched);
        return NULL;
    }
    
    return sched;
}

void scheduler_destroy(Scheduler *sched) {
    if (!sched) {
        return;
    }
    
    /* Free all tasks */
    for (int i = 0; i < sched->task_count; i++) {
        task_destroy(sched->all_tasks[i]);
    }
    free(sched->all_tasks);
    
    /* Free all cgroups */
    for (int i = 0; i < sched->cgroup_count; i++) {
        cgroup_destroy(sched->cgroups[i]);
    }
    free(sched->cgroups);
    
    /* Free heap */
    heap_destroy(sched->runnable_heap);
    
    /* Free CPU queues */
    free(sched->cpu_queues);
    
    free(sched);
}

/* ============================================================================
 * Public Functions - Task Management
 * ============================================================================ */

Task *scheduler_find_task(Scheduler *sched, const char *task_id) {
    if (!sched || !task_id) {
        return NULL;
    }
    
    for (int i = 0; i < sched->task_count; i++) {
        if (strcmp(sched->all_tasks[i]->task_id, task_id) == 0) {
            return sched->all_tasks[i];
        }
    }
    
    return NULL;
}

int scheduler_add_task(Scheduler *sched, Task *task) {
    if (!sched || !task) {
        return -1;
    }
    
    if (sched->task_count >= sched->task_capacity) {
        return -1;  /* Full */
    }
    
    sched->all_tasks[sched->task_count++] = task;
    
    /* Add to runnable heap if task is runnable */
    if (task->state == TASK_STATE_RUNNABLE) {
        heap_insert(sched->runnable_heap, task);
    }
    
    return 0;
}

int scheduler_remove_task(Scheduler *sched, const char *task_id) {
    if (!sched || !task_id) {
        return -1;
    }
    
    for (int i = 0; i < sched->task_count; i++) {
        if (strcmp(sched->all_tasks[i]->task_id, task_id) == 0) {
            Task *task = sched->all_tasks[i];
            
            /* Remove from heap if present */
            if (task->heap_index >= 0) {
                heap_remove(sched->runnable_heap, task);
            }
            
            /* Remove from CPU queue if running */
            for (int j = 0; j < sched->cpu_count; j++) {
                if (sched->cpu_queues[j].current_task == task) {
                    sched->cpu_queues[j].current_task = NULL;
                }
            }
            
            /* Remove from task array */
            task_destroy(task);
            sched->all_tasks[i] = sched->all_tasks[sched->task_count - 1];
            sched->all_tasks[sched->task_count - 1] = NULL;
            sched->task_count--;
            
            return 0;
        }
    }
    
    return -1;
}

/* ============================================================================
 * Public Functions - Cgroup Management
 * ============================================================================ */

Cgroup *scheduler_find_cgroup(Scheduler *sched, const char *cgroup_id) {
    if (!sched || !cgroup_id) {
        return NULL;
    }
    
    for (int i = 0; i < sched->cgroup_count; i++) {
        if (strcmp(sched->cgroups[i]->cgroup_id, cgroup_id) == 0) {
            return sched->cgroups[i];
        }
    }
    
    return NULL;
}

int scheduler_add_cgroup(Scheduler *sched, Cgroup *cgroup) {
    if (!sched || !cgroup) {
        return -1;
    }
    
    if (sched->cgroup_count >= sched->cgroup_capacity) {
        return -1;  /* Full */
    }
    
    sched->cgroups[sched->cgroup_count++] = cgroup;
    return 0;
}

int scheduler_remove_cgroup(Scheduler *sched, const char *cgroup_id) {
    if (!sched || !cgroup_id) {
        return -1;
    }
    
    for (int i = 0; i < sched->cgroup_count; i++) {
        if (strcmp(sched->cgroups[i]->cgroup_id, cgroup_id) == 0) {
            for (int t = 0; t < sched->task_count; t++) {
                Task *task = sched->all_tasks[t];
                if (strcmp(task->cgroup_id, cgroup_id) == 0) {
                    strncpy(task->cgroup_id, "0", MAX_CGROUP_ID_LEN - 1);
                    task->cgroup_id[MAX_CGROUP_ID_LEN - 1] = '\0';
                }
            }

            cgroup_destroy(sched->cgroups[i]);
            sched->cgroups[i] = sched->cgroups[sched->cgroup_count - 1];
            sched->cgroups[sched->cgroup_count - 1] = NULL;
            sched->cgroup_count--;
            return 0;
        }
    }
    
    return -1;
}

/* ============================================================================
 * Public Functions - Event Processing
 * ============================================================================ */

int scheduler_process_event(Scheduler *sched, const Event *event) {
    if (!sched || !event) {
        return -1;
    }
    
    switch (event->action) {
        case EVENT_TASK_CREATE: {
            /* Set initial vruntime to max of current runnable tasks */
            double max_vr = get_max_vruntime(sched);
            
            int nice = event->has_nice ? event->nice : 0;
            Task *task = task_create(event->task_id, nice, 
                                     event->cgroup_id[0] ? event->cgroup_id : NULL);
            if (!task) {
                return -1;
            }
            
            /* New tasks start at max vruntime to prevent starvation of existing tasks */
            task->vruntime = max_vr;
            if (event->has_cpu_mask) {
                task_set_affinity(task, event->cpu_mask, event->cpu_mask_count);
            }
            
            scheduler_add_task(sched, task);
            break;
        }
        
        case EVENT_TASK_EXIT: {
            Task *task = scheduler_find_task(sched, event->task_id);
            if (task) {
                task->state = TASK_STATE_EXITED;
                scheduler_remove_task(sched, event->task_id);
            }
            break;
        }
        
        case EVENT_TASK_BLOCK: {
            Task *task = scheduler_find_task(sched, event->task_id);
            if (task) {
                task->state = TASK_STATE_BLOCKED;
                /* Remove from runnable heap */
                if (task->heap_index >= 0) {
                    heap_remove(sched->runnable_heap, task);
                }
                /* Clear from current CPU */
                if (task->current_cpu >= 0) {
                    sched->cpu_queues[task->current_cpu].current_task = NULL;
                    task->current_cpu = -1;
                }
            }
            break;
        }
        
        case EVENT_TASK_UNBLOCK: {
            Task *task = scheduler_find_task(sched, event->task_id);
            if (task && task->state == TASK_STATE_BLOCKED) {
                task->state = TASK_STATE_RUNNABLE;
                
                /* Set vruntime to min of (current vruntime, min_vruntime - small bonus) */
                /* This gives blocked tasks a slight priority boost */
                double min_vr = get_min_vruntime(sched);
                if (task->vruntime < min_vr - 1.0) {
                    task->vruntime = min_vr - 1.0;  /* Small latency bonus */
                }
                
                heap_insert(sched->runnable_heap, task);
            }
            break;
        }
        
        case EVENT_TASK_YIELD: {
            Task *task = scheduler_find_task(sched, event->task_id);
            if (task) {
                /* Set vruntime to max to give other tasks a chance */
                task->vruntime = get_max_vruntime(sched);
                if (task->heap_index >= 0) {
                    heap_update(sched->runnable_heap, task);
                }
            }
            break;
        }
        
        case EVENT_TASK_SETNICE: {
            Task *task = scheduler_find_task(sched, event->task_id);
            if (task) {
                task_set_nice(task, event->nice);
                /* Heap position may need to change if weight affects scheduling */
            }
            break;
        }
        
        case EVENT_TASK_SET_AFFINITY: {
            Task *task = scheduler_find_task(sched, event->task_id);
            if (task) {
                task_set_affinity(task, event->cpu_mask, event->cpu_mask_count);
            }
            break;
        }
        
        case EVENT_CGROUP_CREATE: {
            int shares = event->has_cpu_shares ? event->cpu_shares : DEFAULT_CPU_SHARES;
            int quota = event->has_cpu_quota ? event->cpu_quota_us : UNLIMITED_QUOTA;
            int period = event->has_cpu_period ? event->cpu_period_us : DEFAULT_CPU_PERIOD_US;
            
            Cgroup *cgroup = cgroup_create(event->cgroup_id, shares, quota, period,
                                           event->cpu_mask, event->cpu_mask_count);
            if (cgroup) {
                cgroup->period_start_vtime = sched->current_vtime;
                scheduler_add_cgroup(sched, cgroup);
            }
            break;
        }
        
        case EVENT_CGROUP_MODIFY: {
            Cgroup *cgroup = scheduler_find_cgroup(sched, event->cgroup_id);
            if (cgroup) {
                int shares = event->has_cpu_shares ? event->cpu_shares : -1;
                int quota = event->has_cpu_quota ? event->cpu_quota_us : -2;
                int period = event->has_cpu_period ? event->cpu_period_us : -2;
                
                if (cgroup_modify(cgroup, shares, quota, period,
                              event->has_cpu_mask ? event->cpu_mask : NULL,
                              event->cpu_mask_count) < 0) {
                    return -1;
                }

                if (event->has_cpu_period && event->cpu_period_us > 0) {
                    cgroup_reset_period(cgroup, sched->current_vtime);
                }
            }
            break;
        }
        
        case EVENT_CGROUP_DELETE: {
            scheduler_remove_cgroup(sched, event->cgroup_id);
            break;
        }
        
        case EVENT_TASK_MOVE_CGROUP: {
            Task *task = scheduler_find_task(sched, event->task_id);
            if (task) {
                strncpy(task->cgroup_id, event->new_cgroup_id, MAX_CGROUP_ID_LEN - 1);
                task->cgroup_id[MAX_CGROUP_ID_LEN - 1] = '\0';
            }
            break;
        }
        
        case EVENT_CPU_BURST: {
            Task *task = scheduler_find_task(sched, event->task_id);
            if (task) {
                task->is_burst = true;
                task->burst_remaining = event->burst_duration;
            }
            break;
        }

        default:
            return -1;
    }
    
    return 0;
}

/* ============================================================================
 * Public Functions - Scheduling
 * ============================================================================ */

SchedulerTick *scheduler_tick(Scheduler *sched, int vtime) {
    if (!sched) {
        return NULL;
    }
    
    sched->current_vtime = vtime;
    sched->preemptions = 0;
    sched->migrations = 0;
    refresh_cgroup_periods(sched, vtime);
    
    /* Allocate result */
    SchedulerTick *tick = calloc(1, sizeof(SchedulerTick));
    if (!tick) {
        return NULL;
    }
    
    tick->vtime = vtime;
    tick->cpu_count = sched->cpu_count;
    tick->schedule = calloc(sched->cpu_count, sizeof(char *));
    if (!tick->schedule) {
        free(tick);
        return NULL;
    }
    
    /* Allocate metadata */
    tick->meta = calloc(1, sizeof(SchedulerMeta));
    if (!tick->meta) {
        free(tick->schedule);
        free(tick);
        return NULL;
    }
    
    /* Track previous task assignment for preemption accounting */
    Task **previous_tasks = calloc(sched->cpu_count, sizeof(Task *));
    bool *assigned = calloc(sched->task_count, sizeof(bool));
    if (!previous_tasks || !assigned) {
        free(previous_tasks);
        free(assigned);
        scheduler_tick_free(tick);
        return NULL;
    }
    
    /* Update vruntime/quota for currently running tasks and return them to runnable state */
    for (int i = 0; i < sched->cpu_count; i++) {
        Task *current = sched->cpu_queues[i].current_task;
        previous_tasks[i] = current;
        if (current && current->state == TASK_STATE_RUNNING) {
            if (!current->is_burst) {
                int effective_weight = get_effective_task_weight(sched, current);
                current->vruntime += calc_vruntime_delta((double)sched->quanta, effective_weight);
            }
            
            if (current->cgroup_id[0] != '\0') {
                Cgroup *cgroup = scheduler_find_cgroup(sched, current->cgroup_id);
                if (cgroup) {
                    double runtime_us = (double)sched->quanta * 1000.0;
                    cgroup_account_runtime(cgroup, runtime_us);
                }
            }
            
            /* Handle burst countdown */
            if (current->is_burst && current->burst_remaining > 0) {
                current->burst_remaining--;
                if (current->burst_remaining == 0) {
                    current->is_burst = false;
                }
            }
            
            current->state = TASK_STATE_RUNNABLE;
        }
        
        /* Reassigned below */
        sched->cpu_queues[i].current_task = NULL;
    }
    
    rebuild_runnable_heap(sched);
    
    /* Track quota usage already committed for this tick (multi-CPU safety) */
    Cgroup *planned_cgroups[MAX_CGROUPS] = {0};
    double planned_runtime_us[MAX_CGROUPS] = {0.0};
    int planned_count = 0;
    double tick_runtime_us = (double)sched->quanta * 1000.0;
    if (tick_runtime_us < 0.0) {
        tick_runtime_us = 0.0;
    }
    
    /* Schedule each CPU using heap minimum selection */
    for (int cpu = 0; cpu < sched->cpu_count; cpu++) {
        Task *previous = previous_tasks[cpu];
        Task *best = pick_task_for_cpu(sched,
                                       cpu,
                                       planned_cgroups,
                                       planned_runtime_us,
                                       &planned_count,
                                       tick_runtime_us);
        
        if (best) {
            for (int i = 0; i < sched->task_count; i++) {
                if (sched->all_tasks[i] == best) {
                    assigned[i] = true;
                    break;
                }
            }
            
            /* Check for preemption */
            if (previous && previous != best) {
                sched->preemptions++;
                if (previous->state == TASK_STATE_RUNNING) {
                    previous->state = TASK_STATE_RUNNABLE;
                }
            }
            
            /* Check for migration */
            if (best->current_cpu >= 0 && best->current_cpu != cpu) {
                sched->migrations++;
            }
            
            /* Assign task to CPU */
            best->current_cpu = cpu;
            best->state = TASK_STATE_RUNNING;
            sched->cpu_queues[cpu].current_task = best;
            tick->schedule[cpu] = strdup(best->task_id);
        } else {
            /* CPU is idle */
            tick->schedule[cpu] = strdup("idle");
        }
    }
    
    /* Runnable tasks left out this tick are no longer assigned to a CPU */
    for (int i = 0; i < sched->task_count; i++) {
        Task *task = sched->all_tasks[i];
        if (!assigned[i] && task->state == TASK_STATE_RUNNABLE) {
            task->current_cpu = -1;
        }
    }
    
    free(previous_tasks);
    free(assigned);
    
    /* Fill metadata */
    tick->meta->preemptions = sched->preemptions;
    tick->meta->migrations = sched->migrations;
    
    /* Count runnable and blocked tasks */
    int runnable_count = 0;
    int blocked_count = 0;
    for (int i = 0; i < sched->task_count; i++) {
        if (sched->all_tasks[i]->state == TASK_STATE_RUNNABLE ||
            sched->all_tasks[i]->state == TASK_STATE_RUNNING) {
            runnable_count++;
        } else if (sched->all_tasks[i]->state == TASK_STATE_BLOCKED) {
            blocked_count++;
        }
    }
    
    tick->meta->runnable_tasks = calloc(runnable_count + 1, sizeof(char *));
    tick->meta->blocked_tasks = calloc(blocked_count + 1, sizeof(char *));
    
    int ri = 0, bi = 0;
    for (int i = 0; i < sched->task_count; i++) {
        if (sched->all_tasks[i]->state == TASK_STATE_RUNNABLE ||
            sched->all_tasks[i]->state == TASK_STATE_RUNNING) {
            tick->meta->runnable_tasks[ri++] = strdup(sched->all_tasks[i]->task_id);
        } else if (sched->all_tasks[i]->state == TASK_STATE_BLOCKED) {
            tick->meta->blocked_tasks[bi++] = strdup(sched->all_tasks[i]->task_id);
        }
    }
    tick->meta->runnable_count = runnable_count;
    tick->meta->blocked_count = blocked_count;
    
    return tick;
}

void scheduler_tick_free(SchedulerTick *tick) {
    if (!tick) {
        return;
    }
    
    if (tick->schedule) {
        for (int i = 0; i < tick->cpu_count; i++) {
            free(tick->schedule[i]);
        }
        free(tick->schedule);
    }
    
    if (tick->meta) {
        for (int i = 0; i < tick->meta->runnable_count; i++) {
            free(tick->meta->runnable_tasks[i]);
        }
        free(tick->meta->runnable_tasks);
        
        for (int i = 0; i < tick->meta->blocked_count; i++) {
            free(tick->meta->blocked_tasks[i]);
        }
        free(tick->meta->blocked_tasks);
        
        free(tick->meta);
    }
    
    free(tick);
}

double scheduler_get_min_vruntime(Scheduler *sched) {
    return get_min_vruntime(sched);
}

double scheduler_get_max_vruntime(Scheduler *sched) {
    return get_max_vruntime(sched);
}
