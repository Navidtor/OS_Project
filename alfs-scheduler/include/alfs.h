/**
 * ALFS - Anushiravan-Level Fair Scheduler
 * Main header file with all type definitions
 */

#ifndef ALFS_H
#define ALFS_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_TASKS 1024
#define MAX_CGROUPS 64
#define MAX_CPUS 128
#define MAX_TASK_ID_LEN 256
#define MAX_CGROUP_ID_LEN 256
#define DEFAULT_SOCKET_PATH "event.socket"
#define NICE_0_WEIGHT 1024
#define DEFAULT_CPU_SHARES 1024
#define DEFAULT_CPU_PERIOD_US 100000  /* 100ms */
#define UNLIMITED_QUOTA -1

/* Nice value range */
#define NICE_MIN -20
#define NICE_MAX 19

/* ============================================================================
 * Linux kernel nice to weight mapping table
 * From: kernel/sched/core.c
 * ============================================================================ */

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

/* wmult values for inverse weight calculation */
static const uint32_t sched_prio_to_wmult[40] = {
    /* -20 */     48388,     59856,     76040,     92818,    118348,
    /* -15 */    147320,    184698,    229616,    287308,    360437,
    /* -10 */    449829,    563644,    704093,    875809,   1099582,
    /*  -5 */   1376151,   1717300,   2157191,   2708050,   3363326,
    /*   0 */   4194304,   5237765,   6557202,   8165337,  10153587,
    /*   5 */  12820798,  15790321,  19976592,  24970740,  31350126,
    /*  10 */  39045157,  49367440,  61356676,  76695844,  95443717,
    /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
};

/* ============================================================================
 * Enumerations
 * ============================================================================ */

typedef enum {
    TASK_STATE_RUNNABLE,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_EXITED
} TaskState;

typedef enum {
    EVENT_INVALID = -1,
    EVENT_TASK_CREATE,
    EVENT_TASK_EXIT,
    EVENT_TASK_BLOCK,
    EVENT_TASK_UNBLOCK,
    EVENT_TASK_YIELD,
    EVENT_TASK_SETNICE,
    EVENT_TASK_SET_AFFINITY,
    EVENT_CGROUP_CREATE,
    EVENT_CGROUP_MODIFY,
    EVENT_CGROUP_DELETE,
    EVENT_TASK_MOVE_CGROUP,
    EVENT_CPU_BURST
} EventAction;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * Task structure representing a process/thread
 */
typedef struct Task {
    char task_id[MAX_TASK_ID_LEN];
    int nice;                       /* -20 to +19, default 0 */
    double vruntime;                /* Virtual runtime */
    int weight;                     /* Computed from nice value */
    TaskState state;
    char cgroup_id[MAX_CGROUP_ID_LEN];
    int *cpu_affinity;              /* Array of allowed CPU IDs */
    int affinity_count;             /* Number of allowed CPUs */
    int current_cpu;                /* Currently assigned CPU (-1 if none) */
    int burst_remaining;            /* Remaining burst duration */
    int heap_index;                 /* Position in heap for O(log n) updates */
    bool is_burst;                  /* True if in CPU burst mode */
} Task;

/**
 * Min-Heap for efficient task scheduling
 */
typedef struct {
    Task **tasks;
    int size;
    int capacity;
} MinHeap;

/**
 * Cgroup structure for resource control
 */
typedef struct Cgroup {
    char cgroup_id[MAX_CGROUP_ID_LEN];
    int cpu_shares;                 /* Default 1024 */
    int cpu_quota_us;               /* Default -1 (unlimited) */
    int cpu_period_us;              /* Default 100000 (100ms) */
    int *cpu_mask;                  /* Array of allowed CPU IDs */
    int cpu_mask_count;
    double quota_used;              /* Track quota usage per period */
    int period_start_vtime;         /* Start of current period */
} Cgroup;

/**
 * Per-CPU run queue
 */
typedef struct {
    int cpu_id;
    Task *current_task;             /* Currently running task */
    double min_vruntime;            /* Minimum vruntime seen */
} CPURunQueue;

/**
 * Scheduler metadata for output
 */
typedef struct {
    int preemptions;                /* Tasks preempted this tick */
    int migrations;                 /* Tasks that changed CPU */
    char **runnable_tasks;
    int runnable_count;
    char **blocked_tasks;
    int blocked_count;
} SchedulerMeta;

/**
 * Scheduler tick output
 */
typedef struct {
    int vtime;
    char **schedule;                /* Task ID per CPU, "idle" if none */
    int cpu_count;
    SchedulerMeta *meta;            /* Optional metadata */
} SchedulerTick;

/**
 * Event structure for incoming events
 */
typedef struct {
    EventAction action;
    char task_id[MAX_TASK_ID_LEN];
    char cgroup_id[MAX_CGROUP_ID_LEN];
    char new_cgroup_id[MAX_CGROUP_ID_LEN];
    int nice;
    int *cpu_mask;
    int cpu_mask_count;
    int cpu_shares;
    int cpu_quota_us;
    int cpu_period_us;
    int burst_duration;
    bool has_nice;
    bool has_cpu_shares;
    bool has_cpu_quota;
    bool has_cpu_period;
    bool has_cpu_mask;
} Event;

/**
 * TimeFrame structure for incoming messages
 */
typedef struct {
    int vtime;
    Event **events;
    int event_count;
} TimeFrame;

/**
 * Main scheduler structure
 */
typedef struct {
    CPURunQueue *cpu_queues;
    int cpu_count;
    int quanta;
    
    /* Task storage using hash table would be better, but using array for simplicity */
    Task **all_tasks;
    int task_count;
    int task_capacity;
    
    /* Cgroup storage */
    Cgroup **cgroups;
    int cgroup_count;
    int cgroup_capacity;
    
    /* Global min-heap for runnable tasks */
    MinHeap *runnable_heap;
    
    /* Current virtual time */
    int current_vtime;
    
    /* Statistics for metadata */
    int preemptions;
    int migrations;
} Scheduler;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* Weight calculation */
static inline int nice_to_weight(int nice) {
    if (nice < NICE_MIN) nice = NICE_MIN;
    if (nice > NICE_MAX) nice = NICE_MAX;
    return sched_prio_to_weight[nice - NICE_MIN];
}

/* vruntime delta calculation */
static inline double calc_vruntime_delta(double runtime, int weight) {
    return runtime * ((double)NICE_0_WEIGHT / (double)weight);
}

#endif /* ALFS_H */
