/** \file
 *  \brief Isotel Sensor Network Reactor
 *  \author <uros@isotel.org>
 */
/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_Reactor Reactor
 * \brief Schedules Timed Events with optional Mutexes, and Event-like Library Calls
 */

#ifndef __ISN_REACTOR_H__
#define __ISN_REACTOR_H__

#include <stddef.h>
#include <stdint.h>

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

/// Ensure minimum delay, so add +1 as we do not know inner position of the timer
#define ISN_REACTOR_DELAY(delay)     (*_isn_reactor_timer + (delay) + 1)

/// Repeat is to be used for periodic tasklet, which on long term does not accumulate errors
#define ISN_REACTOR_REPEAT(delay)    (_isn_reactor_active_timestamp + (delay))

typedef uint32_t isn_reactor_mutex_t;
typedef uint32_t isn_reactor_time_t;
typedef void* (* isn_reactor_tasklet_t)(const void* arg);
typedef void* (* isn_reactor_caller_t)(isn_reactor_tasklet_t tasklet, const void* arg, const void* retval);

extern isn_reactor_time_t const volatile * _isn_reactor_timer;
extern isn_reactor_time_t _isn_reactor_active_timestamp;

typedef struct isn_tasklet_entry {
    isn_reactor_tasklet_t tasklet;
    isn_reactor_caller_t  caller;
    const volatile void*  arg;
    isn_reactor_time_t    time;
} isn_tasklet_entry_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Queue a timed tasklet and follow-up with return or call to another function
 * \returns index in the queue >=0 on success, -1 if queue is full
 */
int isn_reactor_call_at(const isn_reactor_tasklet_t tasklet,
    const isn_reactor_caller_t caller, const void* arg, isn_reactor_time_t timed);

/** Queue a tasklet and follow-up with return or call to another function
 * \returns index in the queue >=0 on success, -1 if queue is full
 */
static inline int isn_reactor_call(const isn_reactor_tasklet_t tasklet,
    const isn_reactor_caller_t caller, const void* arg) {
    return isn_reactor_call_at(tasklet, caller, arg, *_isn_reactor_timer);
}

/** Queue an tasklet
 * \returns index in the queue >=0 on success, -1 if queue is full
 */
static inline int isn_reactor_queue(const isn_reactor_tasklet_t tasklet, const void* arg) {
    return isn_reactor_call_at(tasklet, NULL, arg, *_isn_reactor_timer);
}

/** Queue a timed tasklet
 * \returns index in the queue >=0 on success, -1 if queue is full
 */
static inline int isn_reactor_queue_at(const isn_reactor_tasklet_t tasklet, const void* arg, isn_reactor_time_t timed) {
    return isn_reactor_call_at(tasklet, NULL, arg, timed);
}

/** \returns next free mutex bit, or 0 if none is available */
isn_reactor_mutex_t isn_reactor_getmutex();

/**
 * Assign a mutex to an tasklet in the time of posting it
 *
 * Currently we have 4 mutex groups (bits), 1, 2, 4 and 8.
 * It can be easily extend to 8 and using lower 6 bits for indexing.
 *
 * Example:
 *   isn_reactor_mutexqueue(mytasklet, NULL, 2);
 */
int isn_reactor_mutexqueue(const isn_reactor_tasklet_t tasklet, const void* arg, isn_reactor_mutex_t mutex_bits);

/** Lock given mutex bit(s), one or more at the same time, which will stop execution of
 *  tasklets in the same mutex group
 *
 *  \param mutex_bits obtained with the isn_reactor_getmutex()
 *  \returns 0 on success if lock was obtained, non-zero indiates that this particulr mutex was already locked
 */
int isn_reactor_mutex_lock(isn_reactor_mutex_t mutex_bits);

/** Unlock mutex bits
 * \param mutex_bits obtained with the isn_reactor_getmutex()
 * \returns 0 on success, non-zero if lock was already released
 */
int isn_reactor_mutex_unlock(isn_reactor_mutex_t mutex_bits);

/** Check if locked
 *
 * \param mutex_bits obtained with the isn_reactor_getmutex()
 * \returns non-zero if locked
 */
int isn_reactor_mutex_is_locked(isn_reactor_mutex_t mutex_bits);

/** Is tasklet still pending in the queue, given by exact specs to ensure full integrity
 *
 * \param index returned by any of the above queuing methods, one may also pass invalid index
 * \param tasklet the tasklet, used in validation
 * \param arg used in validation
 * \returns 0 if no-longer in queue or invalid index, and 1 is valid and in the queue
 */
int isn_reactor_isvalid(int index, const isn_reactor_tasklet_t tasklet, const void* arg);

/** Modify timed tasklet, to postpone its execution or request for immediate execution
 * \returns 0 if no-longer in queue or invalid index, and 1 when modified successfully
 */
int isn_reactor_change_timed(int index, const isn_reactor_tasklet_t tasklet, const void* arg, isn_reactor_time_t newtime);

/** Modify time for reoccuring (self-triggered) event, from the event.
 *  So the function modifies the time of the active event and only has effect
 *  if event returns with a pointer to itself.
 */
int isn_reactor_change_timed_self(isn_reactor_time_t newtime);

/** Drop specific tasklet
 * \returns 0 if no-longer in queue or invalid index, and 1 when modified successfully
 */
int isn_reactor_drop(int index, const isn_reactor_tasklet_t tasklet, const void* arg);

/** Drop tasklets from queue of given tasklet and arg
 *  \returns number of removed tasklets
 */
int isn_reactor_dropall(const isn_reactor_tasklet_t tasklet, const void* arg);

/** Execute one tasklet only.
 * \returns Non-zero if any tasklet has been executed
 */
int isn_reactor_step(void);

/** Execute all pending tasklets, enter sleep mode and returns on ISR activity
 * In addition sleep and running times are measured.
 * \returns time to next execution referred to a timer
 */
isn_reactor_time_t isn_reactor_run(void);

/** Self-test, performs basic and mutex queues check
 *  Side effect, it uses one mutex and does not free it.
 * \returns 0 on success, or negative value showing progress
 */
int isn_reactor_selftest();

/** Initialize reactor and provide queue buffer */
void isn_reactor_init(isn_tasklet_entry_t *tasklet_queue, size_t queue_size, const volatile const isn_reactor_time_t* timer);

#endif
