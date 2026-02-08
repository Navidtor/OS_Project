/**
 * ALFS - JSON Handler Implementation
 * Uses cJSON library for JSON parsing and generation
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json_handler.h"
#include "../lib/cJSON/cJSON.h"

/* ============================================================================
 * Action String Mapping
 * ============================================================================ */

static const struct {
    const char *str;
    EventAction action;
} action_map[] = {
    {"TASK_CREATE", EVENT_TASK_CREATE},
    {"TASK_EXIT", EVENT_TASK_EXIT},
    {"TASK_BLOCK", EVENT_TASK_BLOCK},
    {"TASK_UNBLOCK", EVENT_TASK_UNBLOCK},
    {"TASK_YIELD", EVENT_TASK_YIELD},
    {"TASK_SETNICE", EVENT_TASK_SETNICE},
    {"TASK_SET_AFFINITY", EVENT_TASK_SET_AFFINITY},
    {"CGROUP_CREATE", EVENT_CGROUP_CREATE},
    {"CGROUP_MODIFY", EVENT_CGROUP_MODIFY},
    {"CGROUP_DELETE", EVENT_CGROUP_DELETE},
    {"TASK_MOVE_CGROUP", EVENT_TASK_MOVE_CGROUP},
    {"CPU_BURST", EVENT_CPU_BURST},
    {NULL, 0}
};

EventAction json_parse_action(const char *action_str) {
    if (!action_str) {
        return EVENT_INVALID;
    }
    
    for (int i = 0; action_map[i].str != NULL; i++) {
        if (strcmp(action_str, action_map[i].str) == 0) {
            return action_map[i].action;
        }
    }
    
    return EVENT_INVALID;
}

const char *json_action_to_string(EventAction action) {
    for (int i = 0; action_map[i].str != NULL; i++) {
        if (action_map[i].action == action) {
            return action_map[i].str;
        }
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Parse an integer array from cJSON array
 */
static int *parse_int_array(cJSON *array, int *out_count) {
    if (!array || !cJSON_IsArray(array)) {
        *out_count = 0;
        return NULL;
    }
    
    int count = cJSON_GetArraySize(array);
    if (count == 0) {
        *out_count = 0;
        return NULL;
    }
    
    int *result = malloc(sizeof(int) * count);
    if (!result) {
        *out_count = 0;
        return NULL;
    }
    
    int idx = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (cJSON_IsNumber(item)) {
            result[idx++] = item->valueint;
        }
    }
    
    *out_count = idx;
    return result;
}

/**
 * Parse a single event from cJSON object
 */
static Event *parse_event(cJSON *event_json) {
    if (!event_json) {
        return NULL;
    }
    
    Event *event = calloc(1, sizeof(Event));
    if (!event) {
        return NULL;
    }
    
    /* Parse and validate action */
    cJSON *action = cJSON_GetObjectItem(event_json, "action");
    if (!action || !cJSON_IsString(action)) {
        fprintf(stderr, "Invalid event: missing string field 'action'\n");
        free(event);
        return NULL;
    }
    event->action = json_parse_action(action->valuestring);
    if (event->action == EVENT_INVALID) {
        fprintf(stderr, "Invalid event action: %s\n", action->valuestring);
        free(event);
        return NULL;
    }
    
    /* Parse taskId */
    cJSON *task_id = cJSON_GetObjectItem(event_json, "taskId");
    if (task_id && cJSON_IsString(task_id)) {
        strncpy(event->task_id, task_id->valuestring, MAX_TASK_ID_LEN - 1);
    }
    
    /* Parse cgroupId */
    cJSON *cgroup_id = cJSON_GetObjectItem(event_json, "cgroupId");
    if (cgroup_id && cJSON_IsString(cgroup_id)) {
        strncpy(event->cgroup_id, cgroup_id->valuestring, MAX_CGROUP_ID_LEN - 1);
    }
    
    /* Parse newCgroupId */
    cJSON *new_cgroup_id = cJSON_GetObjectItem(event_json, "newCgroupId");
    if (new_cgroup_id && cJSON_IsString(new_cgroup_id)) {
        strncpy(event->new_cgroup_id, new_cgroup_id->valuestring, MAX_CGROUP_ID_LEN - 1);
    }
    
    /* Parse nice */
    cJSON *nice = cJSON_GetObjectItem(event_json, "nice");
    if (nice && cJSON_IsNumber(nice)) {
        event->nice = nice->valueint;
        event->has_nice = true;
    }
    
    /* Parse newNice */
    cJSON *new_nice = cJSON_GetObjectItem(event_json, "newNice");
    if (new_nice && cJSON_IsNumber(new_nice)) {
        event->nice = new_nice->valueint;
        event->has_nice = true;
    }
    
    /* Parse cpuMask */
    cJSON *cpu_mask = cJSON_GetObjectItem(event_json, "cpuMask");
    if (cpu_mask && cJSON_IsArray(cpu_mask)) {
        event->cpu_mask = parse_int_array(cpu_mask, &event->cpu_mask_count);
        event->has_cpu_mask = true;
    }
    
    /* Parse cpuShares */
    cJSON *cpu_shares = cJSON_GetObjectItem(event_json, "cpuShares");
    if (cpu_shares && cJSON_IsNumber(cpu_shares)) {
        event->cpu_shares = cpu_shares->valueint;
        event->has_cpu_shares = true;
    }
    
    /* Parse cpuQuotaUs */
    cJSON *cpu_quota = cJSON_GetObjectItem(event_json, "cpuQuotaUs");
    if (cpu_quota) {
        if (cJSON_IsNumber(cpu_quota)) {
            event->cpu_quota_us = cpu_quota->valueint;
        } else if (cJSON_IsNull(cpu_quota)) {
            event->cpu_quota_us = -1;  /* Null means unlimited */
        }
        event->has_cpu_quota = true;
    }
    
    /* Parse cpuPeriodUs */
    cJSON *cpu_period = cJSON_GetObjectItem(event_json, "cpuPeriodUs");
    if (cpu_period && cJSON_IsNumber(cpu_period)) {
        event->cpu_period_us = cpu_period->valueint;
        event->has_cpu_period = true;
    }
    
    /* Parse duration (for CPU_BURST) */
    cJSON *duration = cJSON_GetObjectItem(event_json, "duration");
    if (duration && cJSON_IsNumber(duration)) {
        event->burst_duration = duration->valueint;
    }
    
    return event;
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

TimeFrame *json_parse_timeframe(const char *json_str) {
    if (!json_str) {
        return NULL;
    }
    
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }
    
    TimeFrame *tf = calloc(1, sizeof(TimeFrame));
    if (!tf) {
        cJSON_Delete(root);
        return NULL;
    }
    
    /* Parse vtime */
    cJSON *vtime = cJSON_GetObjectItem(root, "vtime");
    if (vtime && cJSON_IsNumber(vtime)) {
        tf->vtime = vtime->valueint;
    }
    
    /* Parse events array */
    cJSON *events = cJSON_GetObjectItem(root, "events");
    if (events && cJSON_IsArray(events)) {
        int event_count = cJSON_GetArraySize(events);
        tf->events = malloc(sizeof(Event *) * event_count);
        if (!tf->events) {
            free(tf);
            cJSON_Delete(root);
            return NULL;
        }
        
        tf->event_count = 0;
        cJSON *event_json;
        cJSON_ArrayForEach(event_json, events) {
            Event *event = parse_event(event_json);
            if (event) {
                tf->events[tf->event_count++] = event;
            }
        }
    }
    
    cJSON_Delete(root);
    return tf;
}

void json_free_timeframe(TimeFrame *tf) {
    if (tf) {
        if (tf->events) {
            for (int i = 0; i < tf->event_count; i++) {
                if (tf->events[i]) {
                    free(tf->events[i]->cpu_mask);
                    free(tf->events[i]);
                }
            }
            free(tf->events);
        }
        free(tf);
    }
}

char *json_serialize_tick(const SchedulerTick *tick, bool include_meta) {
    if (!tick) {
        return NULL;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    
    /* Add vtime */
    cJSON_AddNumberToObject(root, "vtime", tick->vtime);
    
    /* Add schedule array */
    cJSON *schedule = cJSON_CreateArray();
    if (!schedule) {
        cJSON_Delete(root);
        return NULL;
    }
    
    for (int i = 0; i < tick->cpu_count; i++) {
        const char *task_id = tick->schedule[i] ? tick->schedule[i] : "idle";
        cJSON_AddItemToArray(schedule, cJSON_CreateString(task_id));
    }
    cJSON_AddItemToObject(root, "schedule", schedule);
    
    /* Add metadata if requested and available */
    if (include_meta && tick->meta) {
        cJSON *meta = cJSON_CreateObject();
        if (meta) {
            cJSON_AddNumberToObject(meta, "preemptions", tick->meta->preemptions);
            cJSON_AddNumberToObject(meta, "migrations", tick->meta->migrations);
            
            /* Add runnable tasks */
            cJSON *runnable = cJSON_CreateArray();
            for (int i = 0; i < tick->meta->runnable_count; i++) {
                cJSON_AddItemToArray(runnable, 
                    cJSON_CreateString(tick->meta->runnable_tasks[i]));
            }
            cJSON_AddItemToObject(meta, "runnableTasks", runnable);
            
            /* Add blocked tasks */
            cJSON *blocked = cJSON_CreateArray();
            for (int i = 0; i < tick->meta->blocked_count; i++) {
                cJSON_AddItemToArray(blocked, 
                    cJSON_CreateString(tick->meta->blocked_tasks[i]));
            }
            cJSON_AddItemToObject(meta, "blockedTasks", blocked);
            
            cJSON_AddItemToObject(root, "meta", meta);
        }
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}
