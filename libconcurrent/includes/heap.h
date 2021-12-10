/// @file pbcombheap.h
/// @author Nikolaos D. Kallimanis
/// @brief This file exposes the API of the a serial heap implementation of fixed size. This heap implementation 
/// supports operations for inserting elements, removing the minimum element and getting the value minimum element.
/// This serial implementation could be combined with almost any combining object provided by the Synch framework
/// (e.g. CC-Synch, Osci, H-Synch, etc) in order to implement fast and efficient concurrent heap implementations.
///
/// @copyright Copyright (c) 2021
#ifndef _HEAP_H_
#define _HEAP_H_

#include <primitives.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>

#define EMPTY_HEAP             LLONG_MIN
#define EMPTY_HEAP_NODE        LLONG_MIN
#define HEAP_INSERT_SUCCESS    0
#define HEAP_INSERT_FAIL       -1
#define HeapElement            uint64_t
#ifndef INITIAL_HEAP_LEVELS
#define INITIAL_HEAP_LEVELS    10
#endif
#define INITIAL_HEAP_SIZE      (1ULL << (INITIAL_HEAP_LEVELS))
#define _SIZE_OF_HEAP_LEVEL(L) (1ULL << (L))
#define _HEAP_INSERT_OP        0x1000000000000000ULL
#define _HEAP_DELETE_MIN_OP    0x2000000000000000ULL
#define _HEAP_GET_MIN_OP       0x3000000000000000ULL
#define _HEAP_OP_MASK          0x7000000000000000ULL
#define _HEAP_VAL_MASK         (~(_HEAP_OP_MASK))

/// @brief HeapState stores the state of an instance of the serial heap implementation. 
/// This heap data-structure is implemented using an array of fixed size.
/// This struct should be initiliazed using the heapInit function.
typedef struct HeapState {
    /// @brief The number of levels that the heap consists of.
    volatile uint32_t levels CACHE_ALIGN;
    /// @brief The last level used of the heap that is currentely used.
    volatile uint32_t last_used_level;
    /// @brief The position with the largest index used in the last level.
    volatile uint32_t last_used_level_pos;
    /// @brief The size of heap's last level.
    volatile uint32_t last_used_level_size;
    /// @brief The i-th element of the array points to the i-th level of the heap.
    HeapElement *heap_arrays[INITIAL_HEAP_LEVELS];
    /// @brief The array that stores the elements of the heap,
    HeapElement bulk[INITIAL_HEAP_SIZE];
} HeapState;

/// @brief This function initializes an instance of the serial heap implementation.
/// This function should be called before any operation is applied to the heap.
inline static void heapInit(HeapState *heap_state);

/// @brief This function provides all the operations (i.e. insert an element, removing the minimum 
/// and getting the minimum) provided by this serial heap implementation.
///
/// @param state A pointer to an instance of the serial heap implementation.
/// @param arg In case that the user wants to perform:
/// (i) Getting the minimum element, arg should be equal to _HEAP_GET_MIN_OP.
/// (ii) Removing the minimum element, arg should be equal to _HEAP_DELETE_MIN_OP.
/// (iii) Inserting a new element ELMNT, arg should be equal to ELMNT | _HEAP_INSERT_OP.
/// In case of inserting a new element ELMNT, the maximum value of ELMNT should be equal 
/// to _HEAP_VAL_MASK.
/// @param pid The pid of the calling thread.
/// @return In case that the user wants to perform:
/// (i) Getting the minimum element; EMPTY_HEAP is returned in case that the heap is empty, 
/// otherwise the value of the minimum element is returned.
/// (ii) Removing the minimum element, EMPTY_HEAP is returned in case that the heap is empty, 
/// otherwise the value of the minimum element is returned.
/// (iii) Inserting a new element; HEAP_INSERT_FAIL is returned in case that there is 
/// no space available in the heap, otherwise HEAP_INSERT_SUCCESS nis returned.
inline static RetVal heapSerialOperation(void *state, ArgVal arg, int pid);

/// @brief Internal function. This should not used directely by the user.
inline static HeapElement serialDeleteMin(HeapState *heap_state);

/// @brief Internal function. This should not used directely by the user.
inline static HeapElement serialInsert(HeapState *heap_state, HeapElement el);

/// @brief Internal function. This should not used directely by the user.
inline static HeapElement serialGetMin(HeapState *heap_state);

/// @brief Internal function. This should not used directely by the user.
inline static void serialCorrectDownHeap(HeapState *heap_state, uint32_t level, uint32_t pos);

/// @brief Internal function. This should not used directely by the user.
inline static void serialCorrectUpHeap(HeapState *heap_state);

inline static void heapInit(HeapState *heap_state) {
    int i;

    heap_state->levels = INITIAL_HEAP_LEVELS;
    heap_state->last_used_level = 0;
    heap_state->last_used_level_pos = 0;
    heap_state->last_used_level_size = 1;
    for (i = 0; i < INITIAL_HEAP_SIZE; i++)
        heap_state->bulk[i] = EMPTY_HEAP_NODE;
    for (i = 0; i < INITIAL_HEAP_LEVELS; i++)
        heap_state->heap_arrays[i] = &heap_state->bulk[(1ULL << i) - 1];
}

inline static void serialCorrectDownHeap(HeapState *heap_state, uint32_t level, uint32_t pos) {
    while (level > 0) {
        uint32_t pos_div_2 = pos / 2;
        if (heap_state->heap_arrays[level][pos] < heap_state->heap_arrays[level - 1][pos_div_2]) {
            HeapElement tmp = heap_state->heap_arrays[level - 1][pos_div_2];
            heap_state->heap_arrays[level - 1][pos_div_2] = heap_state->heap_arrays[level][pos];
            heap_state->heap_arrays[level][pos] = tmp;
        } else {
            break;
        }
        level--;
        pos = pos_div_2;
    }
}

inline static void serialCorrectUpHeap(HeapState *heap_state) {
    uint32_t level = 0;
    uint32_t pos = 0;

    while (level < heap_state->last_used_level) {
        uint32_t pos_left = 2 * pos;
        uint32_t pos_right = 2 * pos + 1;
        if (heap_state->heap_arrays[level][pos] > heap_state->heap_arrays[level + 1][pos_left] || heap_state->heap_arrays[level][pos] > heap_state->heap_arrays[level + 1][pos_right]) {

            if (heap_state->heap_arrays[level + 1][pos_left] > heap_state->heap_arrays[level + 1][pos_right]) {  // Go to the right
                HeapElement tmp = heap_state->heap_arrays[level + 1][pos_right];
                heap_state->heap_arrays[level + 1][pos_right] = heap_state->heap_arrays[level][pos];
                heap_state->heap_arrays[level][pos] = tmp;
                pos = pos_right;
            } else {  // Go to the left
                HeapElement tmp = heap_state->heap_arrays[level + 1][pos_left];
                heap_state->heap_arrays[level + 1][pos_left] = heap_state->heap_arrays[level][pos];
                heap_state->heap_arrays[level][pos] = tmp;
                pos = pos_left;
            }
        } else {
            break;
        }
        level++;
    }
}

inline static HeapElement serialDeleteMin(HeapState *heap_state) {
    HeapElement ret = serialGetMin(heap_state);

    if (ret != EMPTY_HEAP) {
        if (heap_state->last_used_level_pos > 0) {
            heap_state->last_used_level_pos -= 1;
            heap_state->heap_arrays[0][0] = heap_state->heap_arrays[heap_state->last_used_level][heap_state->last_used_level_pos];
            serialCorrectUpHeap(heap_state);
        } else if (heap_state->last_used_level > 0) {
            heap_state->last_used_level -= 1;
            heap_state->last_used_level_size = _SIZE_OF_HEAP_LEVEL(heap_state->last_used_level);
            heap_state->last_used_level_pos = heap_state->last_used_level_size - 2;
            heap_state->heap_arrays[0][0] = heap_state->heap_arrays[heap_state->last_used_level][heap_state->last_used_level_pos];
            serialCorrectUpHeap(heap_state);
        } else if (heap_state->last_used_level == 0) {
            heap_state->last_used_level_pos = 0;
        }
    }

    return ret;
}

inline static HeapElement serialInsert(HeapState *heap_state, HeapElement el) {
    // Check if there is enough space inside the last level
    if (heap_state->last_used_level_pos < heap_state->last_used_level_size) {
        heap_state->heap_arrays[heap_state->last_used_level][heap_state->last_used_level_pos] = el;
        heap_state->last_used_level_pos += 1;
        serialCorrectDownHeap(heap_state, heap_state->last_used_level, heap_state->last_used_level_pos - 1);
        return HEAP_INSERT_SUCCESS;
    } else if (heap_state->last_used_level < heap_state->levels - 1) {  // There is free space, but not in the last used level, There is no need to allocate a new level
        heap_state->last_used_level_size *= 2;
        heap_state->last_used_level += 1;
        heap_state->last_used_level_pos = 1;
        heap_state->heap_arrays[heap_state->last_used_level][0] = el;
        serialCorrectDownHeap(heap_state, heap_state->last_used_level, heap_state->last_used_level_pos - 1);
        return HEAP_INSERT_SUCCESS;
    } else {  // out of space, we need to allocate more levels
#ifdef DEBUG
        fprintf(stderr, "DEBUG: Heap space is full\n");
#endif
        return HEAP_INSERT_FAIL;
    }
}

inline static HeapElement serialGetMin(HeapState *heap_state) {
    if (heap_state->last_used_level != 0 && heap_state->last_used_level_pos != 0) return heap_state->heap_arrays[0][0];
    return EMPTY_HEAP;
}

#ifdef DEBUG
inline static void serialDisplay(HeapState *heap_state) {
    int i, j;

    for (i = 0; i <= heap_state->last_used_level; i++) {
        for (j = 0; j < _SIZE_OF_HEAP_LEVEL(i); j++) {
            if (heap_state->heap_arrays[i][j] >= 0) printf("  %ld  ", heap_state->heap_arrays[i][j]);
        }
        printf("\n");
    }
}
#endif

inline static RetVal heapSerialOperation(void *state, ArgVal arg, int pid) {
    HeapElement ret = EMPTY_HEAP;
    uint64_t op = arg & _HEAP_OP_MASK;
    uint64_t val = arg & _HEAP_VAL_MASK;

    switch (op) {
    case _HEAP_INSERT_OP:
        ret = serialInsert(state, val);
        break;
    case _HEAP_DELETE_MIN_OP:
        ret = serialDeleteMin(state);
        break;
    case _HEAP_GET_MIN_OP:
        ret = serialGetMin(state);
        break;
    default:
        fprintf(stderr, "ERROR: Invalid heap operation\n");
        break;
    }
    return ret;
}

#endif
