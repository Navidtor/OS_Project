/**
 * ALFS - Task Management Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include "task.h"

Task *task_create(const char *task_id, int nice, const char *cgroup_id) {
    if (!task_id) {
        return NULL;
    }
    
    Task *task = calloc(1, sizeof(Task));
    if (!task) {
        return NULL;
    }
    
    strncpy(task->task_id, task_id, MAX_TASK_ID_LEN - 1);
    task->task_id[MAX_TASK_ID_LEN - 1] = '\0';
    
    /* Clamp nice value to valid range */
    if (nice < NICE_MIN) nice = NICE_MIN;
    if (nice > NICE_MAX) nice = NICE_MAX;
    task->nice = nice;
    task->weight = nice_to_weight(nice);
    
    task->vruntime = 0.0;
    task->state = TASK_STATE_RUNNABLE;
    
    if (cgroup_id) {
        strncpy(task->cgroup_id, cgroup_id, MAX_CGROUP_ID_LEN - 1);
        task->cgroup_id[MAX_CGROUP_ID_LEN - 1] = '\0';
    } else {
        strncpy(task->cgroup_id, "0", MAX_CGROUP_ID_LEN - 1);
        task->cgroup_id[MAX_CGROUP_ID_LEN - 1] = '\0';  /* Main/default cgroup */
    }
    
    task->cpu_affinity = NULL;
    task->affinity_count = 0;
    task->current_cpu = -1;
    task->burst_remaining = 0;
    task->is_burst = false;
    task->heap_index = -1;
    
    return task;
}

void task_destroy(Task *task) {
    if (task) {
        free(task->cpu_affinity);
        free(task);
    }
}

int task_set_affinity(Task *task, const int *cpu_mask, int count) {
    if (!task) {
        return -1;
    }
    
    /* Free existing affinity */
    free(task->cpu_affinity);
    task->cpu_affinity = NULL;
    task->affinity_count = 0;
    
    if (cpu_mask && count > 0) {
        task->cpu_affinity = malloc(sizeof(int) * count);
        if (!task->cpu_affinity) {
            return -1;
        }
        memcpy(task->cpu_affinity, cpu_mask, sizeof(int) * count);
        task->affinity_count = count;
    }
    
    return 0;
}

void task_set_nice(Task *task, int nice) {
    if (!task) {
        return;
    }
    
    if (nice < NICE_MIN) nice = NICE_MIN;
    if (nice > NICE_MAX) nice = NICE_MAX;
    
    task->nice = nice;
    task->weight = nice_to_weight(nice);
}

bool task_can_run_on_cpu(const Task *task, int cpu_id) {
    if (!task) {
        return false;
    }
    
    /* No affinity set means can run on any CPU */
    if (task->affinity_count == 0) {
        return true;
    }
    
    for (int i = 0; i < task->affinity_count; i++) {
        if (task->cpu_affinity[i] == cpu_id) {
            return true;
        }
    }
    
    return false;
}

void task_update_vruntime(Task *task, double runtime) {
    if (!task || task->is_burst) {
        return;  /* Don't update vruntime during CPU burst */
    }
    
    double delta = calc_vruntime_delta(runtime, task->weight);
    task->vruntime += delta;
}

void task_set_state(Task *task, TaskState state) {
    if (task) {
        task->state = state;
    }
}

char *task_get_id(const Task *task) {
    if (!task) {
        return NULL;
    }
    return strdup(task->task_id);
}
