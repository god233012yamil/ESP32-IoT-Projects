#ifndef LOCKFREE_RING_H
#define LOCKFREE_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LF_RING_CAPACITY 128U

typedef struct {
    int64_t timestamp_us;
    uint32_t execution_cycles;
    uint32_t release_jitter_us;
    uint32_t sequence;
    bool deadline_missed;
} timing_sample_t;

typedef struct {
    timing_sample_t items[LF_RING_CAPACITY];
    atomic_size_t write_index;
    atomic_size_t read_index;
    atomic_uint_fast32_t dropped_samples;
} lockfree_ring_t;

/**
 * @brief Initializes a single-producer, single-consumer ring buffer.
 *
 * @param ring Ring-buffer instance to initialize.
 */
void lockfree_ring_init(lockfree_ring_t *ring);

/**
 * @brief Pushes one item into the ring buffer.
 *
 * This function is safe for exactly one producer and one consumer.
 *
 * @param ring Ring-buffer instance.
 * @param sample Sample to store.
 * @return true when the item was stored, otherwise false when full.
 */
bool lockfree_ring_push(lockfree_ring_t *ring, const timing_sample_t *sample);

/**
 * @brief Removes one item from the ring buffer.
 *
 * This function is safe for exactly one producer and one consumer.
 *
 * @param ring Ring-buffer instance.
 * @param sample Destination for the retrieved sample.
 * @return true when an item was retrieved, otherwise false when empty.
 */
bool lockfree_ring_pop(lockfree_ring_t *ring, timing_sample_t *sample);

/**
 * @brief Returns the number of samples dropped because the buffer was full.
 *
 * @param ring Ring-buffer instance.
 * @return Number of dropped samples.
 */
uint32_t lockfree_ring_get_dropped(const lockfree_ring_t *ring);

#ifdef __cplusplus
}
#endif

#endif
