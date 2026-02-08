/**
 * ALFS - Min-Heap Unit Tests
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/heap.h"
#include "../include/task.h"

#define TEST_PASS() printf("  [PASS] %s\n", __func__)
#define TEST_FAIL(msg) do { printf("  [FAIL] %s: %s\n", __func__, msg); return 1; } while(0)

/**
 * Test heap creation and destruction
 */
static int test_heap_create_destroy(void) {
    MinHeap *heap = heap_create(10);
    if (!heap) TEST_FAIL("Failed to create heap");
    
    if (heap_size(heap) != 0) TEST_FAIL("New heap should be empty");
    if (!heap_is_empty(heap)) TEST_FAIL("New heap should report as empty");
    
    heap_destroy(heap);
    TEST_PASS();
    return 0;
}

/**
 * Test basic insert and extract
 */
static int test_heap_insert_extract(void) {
    MinHeap *heap = heap_create(10);
    
    Task *t1 = task_create("T1", 0, NULL);
    Task *t2 = task_create("T2", 0, NULL);
    Task *t3 = task_create("T3", 0, NULL);
    
    t1->vruntime = 10.0;
    t2->vruntime = 5.0;
    t3->vruntime = 15.0;
    
    heap_insert(heap, t1);
    heap_insert(heap, t2);
    heap_insert(heap, t3);
    
    if (heap_size(heap) != 3) TEST_FAIL("Heap should have 3 elements");
    
    /* Extract should return minimum vruntime first */
    Task *min = heap_extract_min(heap);
    if (min != t2) TEST_FAIL("First extract should return T2 (vruntime=5)");
    
    min = heap_extract_min(heap);
    if (min != t1) TEST_FAIL("Second extract should return T1 (vruntime=10)");
    
    min = heap_extract_min(heap);
    if (min != t3) TEST_FAIL("Third extract should return T3 (vruntime=15)");
    
    if (!heap_is_empty(heap)) TEST_FAIL("Heap should be empty after extracting all");
    
    task_destroy(t1);
    task_destroy(t2);
    task_destroy(t3);
    heap_destroy(heap);
    
    TEST_PASS();
    return 0;
}

/**
 * Test heap peek
 */
static int test_heap_peek(void) {
    MinHeap *heap = heap_create(10);
    
    if (heap_peek(heap) != NULL) TEST_FAIL("Peek on empty heap should return NULL");
    
    Task *t1 = task_create("T1", 0, NULL);
    t1->vruntime = 10.0;
    heap_insert(heap, t1);
    
    Task *peeked = heap_peek(heap);
    if (peeked != t1) TEST_FAIL("Peek should return T1");
    if (heap_size(heap) != 1) TEST_FAIL("Peek should not remove element");
    
    task_destroy(t1);
    heap_destroy(heap);
    
    TEST_PASS();
    return 0;
}

/**
 * Test heap update (vruntime change)
 */
static int test_heap_update(void) {
    MinHeap *heap = heap_create(10);
    
    Task *t1 = task_create("T1", 0, NULL);
    Task *t2 = task_create("T2", 0, NULL);
    Task *t3 = task_create("T3", 0, NULL);
    
    t1->vruntime = 10.0;
    t2->vruntime = 5.0;
    t3->vruntime = 15.0;
    
    heap_insert(heap, t1);
    heap_insert(heap, t2);
    heap_insert(heap, t3);
    
    /* T2 is currently the minimum */
    if (heap_peek(heap) != t2) TEST_FAIL("T2 should be minimum initially");
    
    /* Update T2's vruntime to be the maximum */
    t2->vruntime = 20.0;
    heap_update(heap, t2);
    
    /* Now T1 should be the minimum */
    if (heap_peek(heap) != t1) TEST_FAIL("T1 should be minimum after update");
    
    task_destroy(t1);
    task_destroy(t2);
    task_destroy(t3);
    heap_destroy(heap);
    
    TEST_PASS();
    return 0;
}

/**
 * Test heap remove
 */
static int test_heap_remove(void) {
    MinHeap *heap = heap_create(10);
    
    Task *t1 = task_create("T1", 0, NULL);
    Task *t2 = task_create("T2", 0, NULL);
    Task *t3 = task_create("T3", 0, NULL);
    
    t1->vruntime = 10.0;
    t2->vruntime = 5.0;
    t3->vruntime = 15.0;
    
    heap_insert(heap, t1);
    heap_insert(heap, t2);
    heap_insert(heap, t3);
    
    /* Remove the middle element (T1) */
    if (heap_remove(heap, t1) != 0) TEST_FAIL("Remove should succeed");
    if (heap_size(heap) != 2) TEST_FAIL("Heap should have 2 elements after remove");
    
    /* T2 should still be minimum */
    Task *min = heap_extract_min(heap);
    if (min != t2) TEST_FAIL("T2 should be minimum after removing T1");
    
    min = heap_extract_min(heap);
    if (min != t3) TEST_FAIL("T3 should be last");
    
    task_destroy(t1);
    task_destroy(t2);
    task_destroy(t3);
    heap_destroy(heap);
    
    TEST_PASS();
    return 0;
}

/**
 * Test heap with many elements
 */
static int test_heap_stress(void) {
    MinHeap *heap = heap_create(16);
    Task *tasks[100];
    
    /* Create tasks with random vruntimes */
    for (int i = 0; i < 100; i++) {
        char name[16];
        snprintf(name, sizeof(name), "T%d", i);
        tasks[i] = task_create(name, 0, NULL);
        tasks[i]->vruntime = (double)(rand() % 1000);
        heap_insert(heap, tasks[i]);
    }
    
    if (heap_size(heap) != 100) TEST_FAIL("Heap should have 100 elements");
    
    /* Extract all and verify ordering */
    double last_vruntime = -1.0;
    for (int i = 0; i < 100; i++) {
        Task *t = heap_extract_min(heap);
        if (!t) TEST_FAIL("Extract should not return NULL");
        if (t->vruntime < last_vruntime) TEST_FAIL("Heap order violation");
        last_vruntime = t->vruntime;
    }
    
    if (!heap_is_empty(heap)) TEST_FAIL("Heap should be empty");
    
    for (int i = 0; i < 100; i++) {
        task_destroy(tasks[i]);
    }
    heap_destroy(heap);
    
    TEST_PASS();
    return 0;
}

/**
 * Run all heap tests
 */
int main(void) {
    printf("Running Min-Heap Tests...\n");
    
    int failures = 0;
    
    failures += test_heap_create_destroy();
    failures += test_heap_insert_extract();
    failures += test_heap_peek();
    failures += test_heap_update();
    failures += test_heap_remove();
    failures += test_heap_stress();
    
    printf("\n");
    if (failures == 0) {
        printf("All heap tests passed!\n");
    } else {
        printf("%d test(s) failed.\n", failures);
    }
    
    return failures;
}
