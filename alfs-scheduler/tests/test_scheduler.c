/**
 * ALFS - Scheduler Unit Tests
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "../include/scheduler.h"
#include "../include/task.h"
#include "../include/cgroup.h"

#define TEST_PASS() printf("  [PASS] %s\n", __func__)
#define TEST_FAIL(msg) do { printf("  [FAIL] %s: %s\n", __func__, msg); return 1; } while(0)

/**
 * Test scheduler initialization
 */
static int test_scheduler_init(void) {
    Scheduler *sched = scheduler_init(4, 1);
    if (!sched) TEST_FAIL("Failed to create scheduler");
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test task creation event
 */
static int test_task_create_event(void) {
    Scheduler *sched = scheduler_init(4, 1);
    
    Event event = {0};
    event.action = EVENT_TASK_CREATE;
    strcpy(event.task_id, "T1");
    event.nice = 0;
    event.has_nice = true;
    
    if (scheduler_process_event(sched, &event) != 0) {
        TEST_FAIL("Failed to process TASK_CREATE event");
    }
    
    Task *task = scheduler_find_task(sched, "T1");
    if (!task) TEST_FAIL("Task T1 not found after creation");
    if (task->nice != 0) TEST_FAIL("Task nice value incorrect");
    if (task->state != TASK_STATE_RUNNABLE) TEST_FAIL("Task should be runnable");
    if (strcmp(task->cgroup_id, "0") != 0) TEST_FAIL("Default cgroup should be \"0\"");
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test basic scheduling
 */
static int test_basic_scheduling(void) {
    Scheduler *sched = scheduler_init(2, 1);
    
    /* Create two tasks */
    Event e1 = {0};
    e1.action = EVENT_TASK_CREATE;
    strcpy(e1.task_id, "T1");
    e1.nice = 0;
    e1.has_nice = true;
    scheduler_process_event(sched, &e1);
    
    Event e2 = {0};
    e2.action = EVENT_TASK_CREATE;
    strcpy(e2.task_id, "T2");
    e2.nice = 0;
    e2.has_nice = true;
    scheduler_process_event(sched, &e2);
    
    /* Run scheduler */
    SchedulerTick *tick = scheduler_tick(sched, 0);
    if (!tick) TEST_FAIL("Failed to create scheduler tick");
    
    /* Both tasks should be scheduled on different CPUs */
    int scheduled_count = 0;
    for (int i = 0; i < tick->cpu_count; i++) {
        if (strcmp(tick->schedule[i], "idle") != 0) {
            scheduled_count++;
        }
    }
    
    if (scheduled_count != 2) TEST_FAIL("Both tasks should be scheduled");
    
    scheduler_tick_free(tick);
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test task blocking and unblocking
 */
static int test_task_block_unblock(void) {
    Scheduler *sched = scheduler_init(1, 1);
    
    /* Create task */
    Event e1 = {0};
    e1.action = EVENT_TASK_CREATE;
    strcpy(e1.task_id, "T1");
    scheduler_process_event(sched, &e1);
    
    /* Block task */
    Event e2 = {0};
    e2.action = EVENT_TASK_BLOCK;
    strcpy(e2.task_id, "T1");
    scheduler_process_event(sched, &e2);
    
    Task *task = scheduler_find_task(sched, "T1");
    if (task->state != TASK_STATE_BLOCKED) TEST_FAIL("Task should be blocked");
    
    /* Run scheduler - should be idle */
    SchedulerTick *tick = scheduler_tick(sched, 0);
    if (strcmp(tick->schedule[0], "idle") != 0) TEST_FAIL("CPU should be idle");
    scheduler_tick_free(tick);
    
    /* Unblock task */
    Event e3 = {0};
    e3.action = EVENT_TASK_UNBLOCK;
    strcpy(e3.task_id, "T1");
    scheduler_process_event(sched, &e3);
    
    if (task->state != TASK_STATE_RUNNABLE) TEST_FAIL("Task should be runnable");
    
    /* Run scheduler - should schedule T1 */
    tick = scheduler_tick(sched, 1);
    if (strcmp(tick->schedule[0], "T1") != 0) TEST_FAIL("T1 should be scheduled");
    scheduler_tick_free(tick);
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test nice values affect scheduling
 */
static int test_nice_values(void) {
    Scheduler *sched = scheduler_init(1, 1);
    
    /* Create high priority task (nice=-10) */
    Event e1 = {0};
    e1.action = EVENT_TASK_CREATE;
    strcpy(e1.task_id, "HIGH");
    e1.nice = -10;
    e1.has_nice = true;
    scheduler_process_event(sched, &e1);
    
    /* Create low priority task (nice=10) */
    Event e2 = {0};
    e2.action = EVENT_TASK_CREATE;
    strcpy(e2.task_id, "LOW");
    e2.nice = 10;
    e2.has_nice = true;
    scheduler_process_event(sched, &e2);
    
    Task *high = scheduler_find_task(sched, "HIGH");
    Task *low = scheduler_find_task(sched, "LOW");
    
    /* High priority task should have higher weight */
    if (high->weight <= low->weight) TEST_FAIL("High priority should have higher weight");
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test CPU affinity
 */
static int test_cpu_affinity(void) {
    Scheduler *sched = scheduler_init(4, 1);
    
    /* Create task with affinity to CPU 0 only */
    Event e1 = {0};
    e1.action = EVENT_TASK_CREATE;
    strcpy(e1.task_id, "T1");
    scheduler_process_event(sched, &e1);
    
    int cpu_mask[] = {0};
    Event e2 = {0};
    e2.action = EVENT_TASK_SET_AFFINITY;
    strcpy(e2.task_id, "T1");
    e2.cpu_mask = cpu_mask;
    e2.cpu_mask_count = 1;
    scheduler_process_event(sched, &e2);
    
    Task *task = scheduler_find_task(sched, "T1");
    if (!task_can_run_on_cpu(task, 0)) TEST_FAIL("Task should be able to run on CPU 0");
    if (task_can_run_on_cpu(task, 1)) TEST_FAIL("Task should not be able to run on CPU 1");
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test cgroup creation
 */
static int test_cgroup_create(void) {
    Scheduler *sched = scheduler_init(4, 1);
    
    int cpu_mask[] = {0, 1, 2, 3};
    Event e1 = {0};
    e1.action = EVENT_CGROUP_CREATE;
    strcpy(e1.cgroup_id, "mygroup");
    e1.cpu_shares = 2048;
    e1.has_cpu_shares = true;
    e1.cpu_quota_us = -1;
    e1.has_cpu_quota = true;
    e1.cpu_period_us = 100000;
    e1.has_cpu_period = true;
    e1.cpu_mask = cpu_mask;
    e1.cpu_mask_count = 4;
    e1.has_cpu_mask = true;
    scheduler_process_event(sched, &e1);
    
    Cgroup *cgroup = scheduler_find_cgroup(sched, "mygroup");
    if (!cgroup) TEST_FAIL("Cgroup not found after creation");
    if (cgroup->cpu_shares != 2048) TEST_FAIL("Cgroup cpu_shares incorrect");
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test task yield
 */
static int test_task_yield(void) {
    Scheduler *sched = scheduler_init(1, 1);
    
    /* Create two tasks */
    Event e1 = {0};
    e1.action = EVENT_TASK_CREATE;
    strcpy(e1.task_id, "T1");
    scheduler_process_event(sched, &e1);
    
    Event e2 = {0};
    e2.action = EVENT_TASK_CREATE;
    strcpy(e2.task_id, "T2");
    scheduler_process_event(sched, &e2);
    
    /* First tick - T1 should run (created first, same vruntime) */
    SchedulerTick *tick = scheduler_tick(sched, 0);
    scheduler_tick_free(tick);
    
    /* T1 yields */
    Event e3 = {0};
    e3.action = EVENT_TASK_YIELD;
    strcpy(e3.task_id, "T1");
    scheduler_process_event(sched, &e3);
    
    /* After yield, T1's vruntime should be max, so T2 runs next */
    tick = scheduler_tick(sched, 1);
    /* T2 should now be scheduled since T1 yielded */
    if (strcmp(tick->schedule[0], "T2") != 0) TEST_FAIL("T2 should be scheduled after T1 yields");
    scheduler_tick_free(tick);
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test task exit
 */
static int test_task_exit(void) {
    Scheduler *sched = scheduler_init(1, 1);
    
    /* Create task */
    Event e1 = {0};
    e1.action = EVENT_TASK_CREATE;
    strcpy(e1.task_id, "T1");
    scheduler_process_event(sched, &e1);
    
    if (!scheduler_find_task(sched, "T1")) TEST_FAIL("Task should exist");
    
    /* Exit task */
    Event e2 = {0};
    e2.action = EVENT_TASK_EXIT;
    strcpy(e2.task_id, "T1");
    scheduler_process_event(sched, &e2);
    
    if (scheduler_find_task(sched, "T1")) TEST_FAIL("Task should not exist after exit");
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test invalid event action is rejected
 */
static int test_invalid_event_action(void) {
    Scheduler *sched = scheduler_init(1, 1);
    if (!sched) TEST_FAIL("Failed to create scheduler");

    Event invalid = {0};
    invalid.action = EVENT_INVALID;

    if (scheduler_process_event(sched, &invalid) == 0) {
        TEST_FAIL("Invalid event action should be rejected");
    }

    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test cgroup quota and period enforcement
 */
static int test_cgroup_quota_enforcement(void) {
    Scheduler *sched = scheduler_init(1, 50);  /* 50ms quanta */
    
    int cpu_mask[] = {0};
    Event cgroup = {0};
    cgroup.action = EVENT_CGROUP_CREATE;
    strcpy(cgroup.cgroup_id, "limited");
    cgroup.cpu_shares = 1024;
    cgroup.has_cpu_shares = true;
    cgroup.cpu_quota_us = 50000;   /* 50ms quota */
    cgroup.has_cpu_quota = true;
    cgroup.cpu_period_us = 100000; /* 100ms period */
    cgroup.has_cpu_period = true;
    cgroup.cpu_mask = cpu_mask;
    cgroup.cpu_mask_count = 1;
    cgroup.has_cpu_mask = true;
    scheduler_process_event(sched, &cgroup);
    
    Event create = {0};
    create.action = EVENT_TASK_CREATE;
    strcpy(create.task_id, "TQ");
    strcpy(create.cgroup_id, "limited");
    scheduler_process_event(sched, &create);
    
    SchedulerTick *tick = scheduler_tick(sched, 0);
    if (strcmp(tick->schedule[0], "TQ") != 0) TEST_FAIL("Task should run at period start");
    scheduler_tick_free(tick);
    
    tick = scheduler_tick(sched, 1);
    if (strcmp(tick->schedule[0], "idle") != 0) TEST_FAIL("Task should be throttled after quota is consumed");
    scheduler_tick_free(tick);
    
    tick = scheduler_tick(sched, 2);
    if (strcmp(tick->schedule[0], "TQ") != 0) TEST_FAIL("Task should run again after period reset");
    scheduler_tick_free(tick);
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test cgroup quota is enforced across multiple CPUs in the same tick
 */
static int test_cgroup_quota_multi_cpu_enforcement(void) {
    Scheduler *sched = scheduler_init(2, 50);  /* 50ms quanta */
    
    int cpu_mask[] = {0, 1};
    Event cgroup = {0};
    cgroup.action = EVENT_CGROUP_CREATE;
    strcpy(cgroup.cgroup_id, "multi");
    cgroup.cpu_shares = 1024;
    cgroup.has_cpu_shares = true;
    cgroup.cpu_quota_us = 50000;   /* Only enough for one CPU for one tick */
    cgroup.has_cpu_quota = true;
    cgroup.cpu_period_us = 100000; /* Reset every 2 ticks at 50ms */
    cgroup.has_cpu_period = true;
    cgroup.cpu_mask = cpu_mask;
    cgroup.cpu_mask_count = 2;
    cgroup.has_cpu_mask = true;
    scheduler_process_event(sched, &cgroup);
    
    Event t1 = {0};
    t1.action = EVENT_TASK_CREATE;
    strcpy(t1.task_id, "A");
    strcpy(t1.cgroup_id, "multi");
    scheduler_process_event(sched, &t1);
    
    Event t2 = {0};
    t2.action = EVENT_TASK_CREATE;
    strcpy(t2.task_id, "B");
    strcpy(t2.cgroup_id, "multi");
    scheduler_process_event(sched, &t2);
    
    SchedulerTick *tick = scheduler_tick(sched, 0);
    int active = 0;
    for (int i = 0; i < tick->cpu_count; i++) {
        if (strcmp(tick->schedule[i], "idle") != 0) {
            active++;
        }
    }
    if (active != 1) TEST_FAIL("Only one CPU should run due to quota limit");
    scheduler_tick_free(tick);
    
    tick = scheduler_tick(sched, 1);
    if (strcmp(tick->schedule[0], "idle") != 0 || strcmp(tick->schedule[1], "idle") != 0) {
        TEST_FAIL("Both CPUs should be throttled until next quota period");
    }
    scheduler_tick_free(tick);
    
    tick = scheduler_tick(sched, 2);
    active = 0;
    for (int i = 0; i < tick->cpu_count; i++) {
        if (strcmp(tick->schedule[i], "idle") != 0) {
            active++;
        }
    }
    if (active != 1) TEST_FAIL("Exactly one CPU should run after period reset");
    scheduler_tick_free(tick);
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test cgroup shares impact scheduling fairness
 */
static int test_cgroup_shares_effect(void) {
    Scheduler *sched = scheduler_init(1, 1);
    int cpu_mask[] = {0};
    
    Event high = {0};
    high.action = EVENT_CGROUP_CREATE;
    strcpy(high.cgroup_id, "high");
    high.cpu_shares = 4096;
    high.has_cpu_shares = true;
    high.cpu_quota_us = -1;
    high.has_cpu_quota = true;
    high.cpu_period_us = 100000;
    high.has_cpu_period = true;
    high.cpu_mask = cpu_mask;
    high.cpu_mask_count = 1;
    high.has_cpu_mask = true;
    scheduler_process_event(sched, &high);
    
    Event low = {0};
    low.action = EVENT_CGROUP_CREATE;
    strcpy(low.cgroup_id, "low");
    low.cpu_shares = 128;
    low.has_cpu_shares = true;
    low.cpu_quota_us = -1;
    low.has_cpu_quota = true;
    low.cpu_period_us = 100000;
    low.has_cpu_period = true;
    low.cpu_mask = cpu_mask;
    low.cpu_mask_count = 1;
    low.has_cpu_mask = true;
    scheduler_process_event(sched, &low);
    
    Event htask = {0};
    htask.action = EVENT_TASK_CREATE;
    strcpy(htask.task_id, "H");
    strcpy(htask.cgroup_id, "high");
    scheduler_process_event(sched, &htask);
    
    Event ltask = {0};
    ltask.action = EVENT_TASK_CREATE;
    strcpy(ltask.task_id, "L");
    strcpy(ltask.cgroup_id, "low");
    scheduler_process_event(sched, &ltask);
    
    int h_runs = 0;
    int l_runs = 0;
    for (int vtime = 0; vtime < 40; vtime++) {
        SchedulerTick *tick = scheduler_tick(sched, vtime);
        if (strcmp(tick->schedule[0], "H") == 0) h_runs++;
        if (strcmp(tick->schedule[0], "L") == 0) l_runs++;
        scheduler_tick_free(tick);
    }
    
    if (h_runs <= l_runs) TEST_FAIL("High-share task should run more often than low-share task");
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test cgroup modify and delete events
 */
static int test_cgroup_modify_delete(void) {
    Scheduler *sched = scheduler_init(2, 1);
    int all_cpus[] = {0, 1};
    int cpu1_only[] = {1};
    
    Event create = {0};
    create.action = EVENT_CGROUP_CREATE;
    strcpy(create.cgroup_id, "grp");
    create.cpu_shares = 1024;
    create.has_cpu_shares = true;
    create.cpu_quota_us = -1;
    create.has_cpu_quota = true;
    create.cpu_period_us = 100000;
    create.has_cpu_period = true;
    create.cpu_mask = all_cpus;
    create.cpu_mask_count = 2;
    create.has_cpu_mask = true;
    scheduler_process_event(sched, &create);

    Event task_create = {0};
    task_create.action = EVENT_TASK_CREATE;
    strcpy(task_create.task_id, "T_GROUP");
    strcpy(task_create.cgroup_id, "grp");
    scheduler_process_event(sched, &task_create);

    SchedulerTick *tick = scheduler_tick(sched, 10);
    scheduler_tick_free(tick);
    
    Event modify = {0};
    modify.action = EVENT_CGROUP_MODIFY;
    strcpy(modify.cgroup_id, "grp");
    modify.cpu_shares = 2048;
    modify.has_cpu_shares = true;
    modify.cpu_quota_us = 50000;
    modify.has_cpu_quota = true;
    modify.cpu_period_us = 200000;
    modify.has_cpu_period = true;
    modify.cpu_mask = cpu1_only;
    modify.cpu_mask_count = 1;
    modify.has_cpu_mask = true;
    scheduler_process_event(sched, &modify);
    
    Cgroup *group = scheduler_find_cgroup(sched, "grp");
    if (!group) TEST_FAIL("Cgroup should exist after modify");
    if (group->cpu_shares != 2048) TEST_FAIL("Cgroup shares should be updated");
    if (group->cpu_quota_us != 50000) TEST_FAIL("Cgroup quota should be updated");
    if (group->cpu_period_us != 200000) TEST_FAIL("Cgroup period should be updated");
    if (group->period_start_vtime != 10) TEST_FAIL("Cgroup period should reset at current vtime");
    if (group->cpu_mask_count != 1 || group->cpu_mask[0] != 1) TEST_FAIL("Cgroup mask should be updated");
    
    Event del = {0};
    del.action = EVENT_CGROUP_DELETE;
    strcpy(del.cgroup_id, "grp");
    scheduler_process_event(sched, &del);
    
    if (scheduler_find_cgroup(sched, "grp")) TEST_FAIL("Cgroup should be deleted");

    Task *task = scheduler_find_task(sched, "T_GROUP");
    if (!task) TEST_FAIL("Task should still exist after cgroup deletion");
    if (strcmp(task->cgroup_id, "0") != 0) TEST_FAIL("Task should be reassigned to default cgroup");
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test moving task between cgroups changes CPU eligibility
 */
static int test_task_move_cgroup(void) {
    Scheduler *sched = scheduler_init(2, 1);
    int cpu0[] = {0};
    int cpu1[] = {1};
    
    Event cg_a = {0};
    cg_a.action = EVENT_CGROUP_CREATE;
    strcpy(cg_a.cgroup_id, "A");
    cg_a.cpu_shares = 1024;
    cg_a.has_cpu_shares = true;
    cg_a.cpu_quota_us = -1;
    cg_a.has_cpu_quota = true;
    cg_a.cpu_period_us = 100000;
    cg_a.has_cpu_period = true;
    cg_a.cpu_mask = cpu0;
    cg_a.cpu_mask_count = 1;
    cg_a.has_cpu_mask = true;
    scheduler_process_event(sched, &cg_a);
    
    Event cg_b = {0};
    cg_b.action = EVENT_CGROUP_CREATE;
    strcpy(cg_b.cgroup_id, "B");
    cg_b.cpu_shares = 1024;
    cg_b.has_cpu_shares = true;
    cg_b.cpu_quota_us = -1;
    cg_b.has_cpu_quota = true;
    cg_b.cpu_period_us = 100000;
    cg_b.has_cpu_period = true;
    cg_b.cpu_mask = cpu1;
    cg_b.cpu_mask_count = 1;
    cg_b.has_cpu_mask = true;
    scheduler_process_event(sched, &cg_b);
    
    Event create = {0};
    create.action = EVENT_TASK_CREATE;
    strcpy(create.task_id, "TM");
    strcpy(create.cgroup_id, "A");
    scheduler_process_event(sched, &create);
    
    SchedulerTick *tick = scheduler_tick(sched, 0);
    if (strcmp(tick->schedule[0], "TM") != 0) TEST_FAIL("Task should start on CPU 0 via cgroup A");
    scheduler_tick_free(tick);
    
    Event move = {0};
    move.action = EVENT_TASK_MOVE_CGROUP;
    strcpy(move.task_id, "TM");
    strcpy(move.new_cgroup_id, "B");
    scheduler_process_event(sched, &move);
    
    tick = scheduler_tick(sched, 1);
    if (strcmp(tick->schedule[0], "idle") != 0) TEST_FAIL("CPU 0 should be idle after moving task to cgroup B");
    if (strcmp(tick->schedule[1], "TM") != 0) TEST_FAIL("Task should move to CPU 1 via cgroup B");
    scheduler_tick_free(tick);
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Test CPU_BURST disables vruntime updates for burst duration
 */
static int test_cpu_burst_vruntime(void) {
    Scheduler *sched = scheduler_init(1, 1);
    
    Event create = {0};
    create.action = EVENT_TASK_CREATE;
    strcpy(create.task_id, "B1");
    scheduler_process_event(sched, &create);
    
    SchedulerTick *tick = scheduler_tick(sched, 0);
    scheduler_tick_free(tick);
    
    tick = scheduler_tick(sched, 1);
    scheduler_tick_free(tick);
    
    Task *task = scheduler_find_task(sched, "B1");
    if (!task) TEST_FAIL("Task should exist");
    double before_burst = task->vruntime;
    
    Event burst = {0};
    burst.action = EVENT_CPU_BURST;
    strcpy(burst.task_id, "B1");
    burst.burst_duration = 2;
    scheduler_process_event(sched, &burst);
    
    tick = scheduler_tick(sched, 2);
    scheduler_tick_free(tick);
    tick = scheduler_tick(sched, 3);
    scheduler_tick_free(tick);
    
    if (fabs(task->vruntime - before_burst) > 1e-9) {
        TEST_FAIL("Vruntime should not change during CPU burst");
    }
    
    tick = scheduler_tick(sched, 4);
    scheduler_tick_free(tick);
    
    if (task->vruntime <= before_burst) {
        TEST_FAIL("Vruntime should increase after burst finishes");
    }
    
    scheduler_destroy(sched);
    TEST_PASS();
    return 0;
}

/**
 * Run all scheduler tests
 */
int main(void) {
    printf("Running Scheduler Tests...\n");
    
    int failures = 0;
    
    failures += test_scheduler_init();
    failures += test_task_create_event();
    failures += test_basic_scheduling();
    failures += test_task_block_unblock();
    failures += test_nice_values();
    failures += test_cpu_affinity();
    failures += test_cgroup_create();
    failures += test_task_yield();
    failures += test_task_exit();
    failures += test_invalid_event_action();
    failures += test_cgroup_quota_enforcement();
    failures += test_cgroup_quota_multi_cpu_enforcement();
    failures += test_cgroup_shares_effect();
    failures += test_cgroup_modify_delete();
    failures += test_task_move_cgroup();
    failures += test_cpu_burst_vruntime();
    
    printf("\n");
    if (failures == 0) {
        printf("All scheduler tests passed!\n");
    } else {
        printf("%d test(s) failed.\n", failures);
    }
    
    return failures;
}
