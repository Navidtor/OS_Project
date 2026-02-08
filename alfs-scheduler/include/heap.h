/**
 * ALFS - Min-Heap Interface
 * Efficient O(log n) insert and extract-min operations
 */

#ifndef HEAP_H
#define HEAP_H

#include "alfs.h"

/**
 * Create a new min-heap with given initial capacity
 * @param capacity Initial capacity
 * @return Pointer to new MinHeap or NULL on failure
 */
MinHeap *heap_create(int capacity);

/**
 * Destroy a min-heap and free all memory
 * Note: Does NOT free the tasks themselves
 * @param heap Heap to destroy
 */
void heap_destroy(MinHeap *heap);

/**
 * Insert a task into the heap
 * @param heap Target heap
 * @param task Task to insert
 * @return 0 on success, -1 on failure
 */
int heap_insert(MinHeap *heap, Task *task);

/**
 * Extract and return the task with minimum vruntime
 * @param heap Source heap
 * @return Task with minimum vruntime, or NULL if heap is empty
 */
Task *heap_extract_min(MinHeap *heap);

/**
 * Peek at the task with minimum vruntime without removing it
 * @param heap Source heap
 * @return Task with minimum vruntime, or NULL if heap is empty
 */
Task *heap_peek(MinHeap *heap);

/**
 * Update a task's position after vruntime change
 * The task must already be in the heap
 * @param heap Target heap
 * @param task Task that was updated
 */
void heap_update(MinHeap *heap, Task *task);

/**
 * Remove a specific task from the heap
 * @param heap Target heap
 * @param task Task to remove
 * @return 0 on success, -1 if task not found
 */
int heap_remove(MinHeap *heap, Task *task);

/**
 * Check if heap is empty
 * @param heap Heap to check
 * @return true if empty, false otherwise
 */
bool heap_is_empty(MinHeap *heap);

/**
 * Get heap size
 * @param heap Heap to check
 * @return Number of elements in heap
 */
int heap_size(MinHeap *heap);

/**
 * Find a task in the heap by task_id
 * @param heap Heap to search
 * @param task_id Task ID to find
 * @return Task if found, NULL otherwise
 */
Task *heap_find(MinHeap *heap, const char *task_id);

#endif /* HEAP_H */
