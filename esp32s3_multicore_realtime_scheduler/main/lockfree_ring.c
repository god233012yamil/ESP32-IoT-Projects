#include "lockfree_ring.h"

#include <string.h>

/**
 * @brief Initializes a single-producer, single-consumer ring buffer.
 *
 * @param ring Ring-buffer instance to initialize.
 */
void lockfree_ring_init(lockfree_ring_t *ring)
{
    if (ring == NULL) {
        return;
    }

    memset(ring->items, 0, sizeof(ring->items));
    atomic_init(&ring->write_index, 0U);
    atomic_init(&ring->read_index, 0U);
    atomic_init(&ring->dropped_samples, 0U);
}

/**
 * @brief Pushes one item into the ring buffer.
 *
 * This function is safe for exactly one producer and one consumer.
 *
 * @param ring Ring-buffer instance.
 * @param sample Sample to store.
 * @return true when the item was stored, otherwise false when full.
 */
bool lockfree_ring_push(lockfree_ring_t *ring, const timing_sample_t *sample)
{
    if ((ring == NULL) || (sample == NULL)) {
        return false;
    }

    const size_t write_index = atomic_load_explicit(
        &ring->write_index,
        memory_order_relaxed);
    const size_t next_index = (write_index + 1U) % LF_RING_CAPACITY;
    const size_t read_index = atomic_load_explicit(
        &ring->read_index,
        memory_order_acquire);

    if (next_index == read_index) {
        atomic_fetch_add_explicit(
            &ring->dropped_samples,
            1U,
            memory_order_relaxed);
        return false;
    }

    ring->items[write_index] = *sample;
    atomic_store_explicit(
        &ring->write_index,
        next_index,
        memory_order_release);

    return true;
}

/**
 * @brief Removes one item from the ring buffer.
 *
 * This function is safe for exactly one producer and one consumer.
 *
 * @param ring Ring-buffer instance.
 * @param sample Destination for the retrieved sample.
 * @return true when an item was retrieved, otherwise false when empty.
 */
bool lockfree_ring_pop(lockfree_ring_t *ring, timing_sample_t *sample)
{
    if ((ring == NULL) || (sample == NULL)) {
        return false;
    }

    const size_t read_index = atomic_load_explicit(
        &ring->read_index,
        memory_order_relaxed);
    const size_t write_index = atomic_load_explicit(
        &ring->write_index,
        memory_order_acquire);

    if (read_index == write_index) {
        return false;
    }

    *sample = ring->items[read_index];
    atomic_store_explicit(
        &ring->read_index,
        (read_index + 1U) % LF_RING_CAPACITY,
        memory_order_release);

    return true;
}

/**
 * @brief Returns the number of samples dropped because the buffer was full.
 *
 * @param ring Ring-buffer instance.
 * @return Number of dropped samples.
 */
uint32_t lockfree_ring_get_dropped(const lockfree_ring_t *ring)
{
    if (ring == NULL) {
        return 0U;
    }

    return (uint32_t)atomic_load_explicit(
        &ring->dropped_samples,
        memory_order_relaxed);
}
