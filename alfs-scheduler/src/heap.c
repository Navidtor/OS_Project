/**
 * ALFS - Min-Heap Implementation
 * 
 * This min-heap is optimized for the CFS scheduler:
 * - O(log n) insert
 * - O(log n) extract-min
 * - O(1) peek
 * - O(log n) update (using heap_index)
 * - O(log n) remove (using heap_index)
 */

#include <stdlib.h>
#include <string.h>
#include "heap.h"

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline int parent(int i) { return (i - 1) / 2; }
static inline int left_child(int i) { return 2 * i + 1; }
static inline int right_child(int i) { return 2 * i + 2; }

/**
 * Swap two tasks in the heap and update their indices
 */
static void heap_swap(MinHeap *heap, int i, int j) {
    Task *temp = heap->tasks[i];
    heap->tasks[i] = heap->tasks[j];
    heap->tasks[j] = temp;
    
    heap->tasks[i]->heap_index = i;
    heap->tasks[j]->heap_index = j;
}

/**
 * Bubble up a task to maintain heap property
 */
static void heap_bubble_up(MinHeap *heap, int idx) {
    while (idx > 0 && heap->tasks[parent(idx)]->vruntime > heap->tasks[idx]->vruntime) {
        heap_swap(heap, idx, parent(idx));
        idx = parent(idx);
    }
}

/**
 * Bubble down a task to maintain heap property
 */
static void heap_bubble_down(MinHeap *heap, int idx) {
    int min_idx = idx;
    int left = left_child(idx);
    int right = right_child(idx);
    
    if (left < heap->size && heap->tasks[left]->vruntime < heap->tasks[min_idx]->vruntime) {
        min_idx = left;
    }
    
    if (right < heap->size && heap->tasks[right]->vruntime < heap->tasks[min_idx]->vruntime) {
        min_idx = right;
    }
    
    if (min_idx != idx) {
        heap_swap(heap, idx, min_idx);
        heap_bubble_down(heap, min_idx);
    }
}

/**
 * Ensure heap has enough capacity
 */
static int heap_ensure_capacity(MinHeap *heap) {
    if (heap->size >= heap->capacity) {
        int new_capacity = heap->capacity * 2;
        Task **new_tasks = realloc(heap->tasks, sizeof(Task *) * new_capacity);
        if (!new_tasks) {
            return -1;
        }
        heap->tasks = new_tasks;
        heap->capacity = new_capacity;
    }
    return 0;
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

MinHeap *heap_create(int capacity) {
    MinHeap *heap = malloc(sizeof(MinHeap));
    if (!heap) {
        return NULL;
    }
    
    heap->tasks = malloc(sizeof(Task *) * capacity);
    if (!heap->tasks) {
        free(heap);
        return NULL;
    }
    
    heap->size = 0;
    heap->capacity = capacity;
    
    return heap;
}

void heap_destroy(MinHeap *heap) {
    if (heap) {
        free(heap->tasks);
        free(heap);
    }
}

int heap_insert(MinHeap *heap, Task *task) {
    if (!heap || !task) {
        return -1;
    }
    
    if (heap_ensure_capacity(heap) < 0) {
        return -1;
    }
    
    /* Insert at the end */
    int idx = heap->size;
    heap->tasks[idx] = task;
    task->heap_index = idx;
    heap->size++;
    
    /* Bubble up to maintain heap property */
    heap_bubble_up(heap, idx);
    
    return 0;
}

Task *heap_extract_min(MinHeap *heap) {
    if (!heap || heap->size == 0) {
        return NULL;
    }
    
    Task *min_task = heap->tasks[0];
    min_task->heap_index = -1;
    
    /* Move last element to root */
    heap->size--;
    if (heap->size > 0) {
        heap->tasks[0] = heap->tasks[heap->size];
        heap->tasks[0]->heap_index = 0;
        heap_bubble_down(heap, 0);
    }
    
    return min_task;
}

Task *heap_peek(MinHeap *heap) {
    if (!heap || heap->size == 0) {
        return NULL;
    }
    return heap->tasks[0];
}

void heap_update(MinHeap *heap, Task *task) {
    if (!heap || !task || task->heap_index < 0 || task->heap_index >= heap->size) {
        return;
    }
    
    int idx = task->heap_index;
    
    /* Try bubbling up first, then down */
    if (idx > 0 && heap->tasks[parent(idx)]->vruntime > task->vruntime) {
        heap_bubble_up(heap, idx);
    } else {
        heap_bubble_down(heap, idx);
    }
}

int heap_remove(MinHeap *heap, Task *task) {
    if (!heap || !task || task->heap_index < 0 || task->heap_index >= heap->size) {
        return -1;
    }
    
    int idx = task->heap_index;
    task->heap_index = -1;
    
    /* Move last element to this position */
    heap->size--;
    if (idx < heap->size) {
        heap->tasks[idx] = heap->tasks[heap->size];
        heap->tasks[idx]->heap_index = idx;
        
        /* Restore heap property */
        if (idx > 0 && heap->tasks[parent(idx)]->vruntime > heap->tasks[idx]->vruntime) {
            heap_bubble_up(heap, idx);
        } else {
            heap_bubble_down(heap, idx);
        }
    }
    
    return 0;
}

bool heap_is_empty(MinHeap *heap) {
    return !heap || heap->size == 0;
}

int heap_size(MinHeap *heap) {
    return heap ? heap->size : 0;
}

Task *heap_find(MinHeap *heap, const char *task_id) {
    if (!heap || !task_id) {
        return NULL;
    }
    
    for (int i = 0; i < heap->size; i++) {
        if (strcmp(heap->tasks[i]->task_id, task_id) == 0) {
            return heap->tasks[i];
        }
    }
    
    return NULL;
}
