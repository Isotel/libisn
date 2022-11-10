/** \file
 *  \brief Isotel Sensor Network Reactor
 *  \author <uros@isotel.org>
 */
/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_Reactor Reactor
 * \brief Schedules Timed Events with optional Mutexes, and Event-like Library Calls
 *
 * \section Concept
 *
 * Reactor is a tiny and efficient event-based scheduler, with single or multiple
 * priority queues, timed execution and mutexes. Rather than having a polling based
 * software architecture, timed, event-based execution implicitly provides options
 * to save power, minimizes stack usage, and instead of try-and-fail resource
 * allocation strategies uses forward mutex locking/release mechanism, reducing
 * CPU and stack overhead that would otherwise be required with pre-emptive OS.
 *
 * \subsection Interrupts and Events
 *
 * Interrupts are a kind of CPU primary pre-emptive (nested) events, to ensure
 * low-latency response to peripheral modules. In this same principle events are
 * extended to user space software, to handle more longer in execution time tasks.
 * Events may be spawned from interrupts and user space, by simply adding it to the
 * queue:
 *
 * Example...
 *
 * Reactor may provide different queues. Since interrupts are there to handle
 * highest priority load, typically user space only requires two periority queues,
 * the default, and higher priority (low-latency) queue. In addition reactor may
 * be asked about other higher priority events, and cpu friendly events may
 * exit some for() loop earlier to give CPU space to other events, i.e., some
 * back-ground processing events.
 *
 * Event based software is easy to write, easy to read and maintain, easily portable
 * as they depend on a very few library calls.
 *
 * \subsection Timed Execution
 *
 * MCU based application often require precise timed execution.
 * Each event can be assigned absolute time of triggering, and after triggering,
 * it may be retriggered without overhead of deletion and reinsertion of the event.
 * Event may query for an absolute time (latency) to be able to re-use this
 * information in processing.
 *
 * Example of single shot event, delayed execution:
 *
 * Example of period event:
 *
 *
 * Timed event simplify single-shot and periodic timers with absolute long-term
 * accuracy.
 *
 * \subsection Mutexes
 *
 * A communication interface is often shared among different devices, as i.e. I2C,
 * SPI bus, external memory, and so on. To avoid collisions, a mutex is assigned
 * and peripheral locks the execution of further events until it completes the
 * transaction. No other event assigned to the same mutex would execute till
 * unlocked.
 *
 * In a similar way mutex can be used to prevent buffer overful, i.e. an UART
 * transmit buffer may lock some mutex once full, and until data are transmitted.
 * Instead of having failed trials of transmission, events assigned with this
 * same mutex would not execute until UART is ready to receive more data.
 * Similarly it can be used in reverse direction. UART receive may not forward
 * a frame to underlaying event (processor) until it is free. In this case
 * obviously UART receieve buffer must be large enough to store all the data,
 * or it may simply drop, and mark such overrun condition.
 *
 * \subsection Channels, Multi-Cores
 *
 * The isn_reactor_channel() queues an event via non-blocking channel, to be
 * executed at other core, which queue is provided as a variadic NULL terminated
 * list of queues in the isn_reactor_runall(queue0, queue1, NULL) function.
 * Channels are FIFO queues and needs to be initialized first with the
 * isn_reactor_initchannel(). Take care that initialization is done prior
 * using it with either isn_reactor_initchannel() or isn_reactor_runall()
 * calls.
 * If a processor core is using sleep mode, then anther processor needs to
 * wake it up. The isn_reactor_setchannel_handler() sets a wake-up handler
 * that is called each time an event is pushed into the channel, which
 * implementation is custom; i.e. if using isn_clock_wfi() then use libs
 * provided handler: isn_clock_foreign_wakeup().
 *
 * In a multi core system some events may not be used by local processor,
 * and would be removed by the gcc unless kept in keep section. For this
 * reason KEEP_EVENT attribute is provided, however, the section must
 * be added to the linker script, i.e. for PSoC6 M4, modify file
 * dfu_cm4.ld:
 * ~~~
.text :
    {
        . = ALIGN(4);
        __Vectors = . ;
        KEEP(*(.vectors))
        . = ALIGN(4);
        __Vectors_End = .;
        __Vectors_Size = __Vectors_End - __Vectors;
        __end__ = .;

        . = ALIGN(4);
        *(.text*)

        KEEP(*(.events))
        KEEP(*(.init))
        KEEP(*(.fini))

 * ~~~
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2021, Isotel, http://isotel.org
 */

/* Notes
=======================================================================================================
m0 -> start m4
         ---> start adc
         ---> start trigger
      merge and
m0 <- return

simple solution is if start adc, trigger are simple ordered events, to pass return callback to it,
which then terminates the return to m0. Merge can be seen as the last ordered call which then returns.

another solution would be that start m4 sets group ref counter, to which start adc, trigger are
added, and when all three are done, return is implemented; i.e. suitable for unordered events and
multi-core but missing merge.
The problem is, where is this reference counter or atomic or bit-bang mem? to keep code simple.

*/

#ifndef __ISN_REACTOR_H__
#define __ISN_REACTOR_H__

#include <stddef.h>
#include <stdint.h>
#include "isn_clock.h"

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

#define ISN_EVENT(f)                    (isn_reactor_tasklet_t)f

#define ISN_REACTOR_TASKLET_INVALID     -1

/// Ensure minimum delay, so add +1 as we do not know inner position of the timer
#define ISN_REACTOR_DELAY_ticks(delay)  (ISN_CLOCK_NOW + (delay) + 1)
#define ISN_REACTOR_DELAY_s(delay)      (ISN_CLOCK_NOW + ISN_CLOCK_s(delay) + 1)
#define ISN_REACTOR_DELAY_ms(delay)     (ISN_CLOCK_NOW + ISN_CLOCK_ms(delay) + 1)
#define ISN_REACTOR_DELAY_us(delay)     (ISN_CLOCK_NOW + ISN_CLOCK_us(delay) + 1)

/// Repeat is to be used for periodic tasklet, which on long term does not accumulate errors
#define ISN_REACTOR_REPEAT_ticks(period) (_isn_reactor_active_timestamp + (period))
#define ISN_REACTOR_REPEAT_s(period)    (_isn_reactor_active_timestamp + ISN_CLOCK_s(period))
#define ISN_REACTOR_REPEAT_ms(period)   (_isn_reactor_active_timestamp + ISN_CLOCK_ms(period))
#define ISN_REACTOR_REPEAT_us(period)   (_isn_reactor_active_timestamp + ISN_CLOCK_us(period))

typedef uint32_t isn_reactor_mutex_t;
typedef void* (* isn_reactor_tasklet_t)(void* arg);
typedef int (* isn_reactor_queue_t)(const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed, isn_reactor_mutex_t mutex_bits);

extern isn_clock_counter_t _isn_reactor_active_timestamp;

struct isn_tasklet_queue;
typedef struct isn_tasklet_entry {
    isn_reactor_tasklet_t tasklet;
    isn_reactor_tasklet_t caller;
    struct isn_tasklet_queue* caller_queue; ///< Cross-cpu calling back mecninism
    void                 *arg;
    isn_clock_counter_t   time;
} isn_tasklet_entry_t;

typedef struct isn_tasklet_queue {
    volatile size_t wri, rdi;
    isn_tasklet_entry_t* fifo;
    size_t size_mask;
    void (*wakeup)(void);
} isn_tasklet_queue_t;

#define ISN_TASKLET_QUEUE_INIT  { 0, 0, NULL, 0, NULL }

/*----------------------------------------------------------------------*/
/* Public Aliases                                                       */
/*----------------------------------------------------------------------*/

/** System-space, preemptive, highest priority queue, for low-latency support just below the interrupts */
int isn_reactor_systemqueue(const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed, isn_reactor_mutex_t mutex_bits);

/** User-space, non-preemptive, priority queue, to promptly respond to interrupts */
int isn_reactor_priorityqueue(const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed, isn_reactor_mutex_t mutex_bits);

/** User-space, non-preemptive, normal priority queue, used by most of the reactor queue calls */
int isn_reactor_userqueue(const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed, isn_reactor_mutex_t mutex_bits);

/** User-space, non-preemptive, back-ground queue, for back-ground processing, see isn_reactor_is_last() to pre-empt manually */
int isn_reactor_backqueue(const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed, isn_reactor_mutex_t mutex_bits);

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/* Multi-Processor Channels - Non-blocking Queues */

#define KEEP_EVENT __attribute__((section(".events")))

/** Initialiaze a channel */
void isn_reactor_initchannel(isn_tasklet_queue_t *queue, isn_tasklet_entry_t* fifobuf, size_t size_mask);

inline static void isn_reactor_setchannel_handler(isn_tasklet_queue_t *queue, void (*wakeup)(void)) {queue->wakeup = wakeup;};

/** Post a timed event to a channel, to be retrieved by some other processor in a system */
int isn_reactor_channel_at(isn_tasklet_queue_t *queue, const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed);

/** Post a timed event to a channel, to be retrieved by some other processor in a system */
int isn_reactor_channel_call_at(isn_tasklet_queue_t *queue, const isn_reactor_tasklet_t tasklet,
    isn_tasklet_queue_t *caller_queue, const isn_reactor_tasklet_t caller,
    void* arg, isn_clock_counter_t timed);

static inline void isn_reactor_wakeup_channel(isn_tasklet_queue_t *queue) {
    if (queue->rdi != queue->wri && queue->wakeup) queue->wakeup(); 
}

/* Processor Local */

/** Queue a timed tasklet and follow-up with return or call to another function
 * \returns index in the queue >=0 on success, -1 if queue is full
 */
int isn_reactor_call_at(const isn_reactor_tasklet_t tasklet,
    const isn_reactor_tasklet_t caller, void* arg, isn_clock_counter_t timed);

/** Queue a tasklet and follow-up with return or call to another function
 * \returns index in the queue >=0 on success, -1 if queue is full
 */
static inline int isn_reactor_call(const isn_reactor_tasklet_t tasklet,
    const isn_reactor_tasklet_t caller, void* arg) {
    return isn_reactor_call_at(tasklet, caller, arg, ISN_CLOCK_NOW);
}

/** Queues an event and passes further its callee.
 *  This function must be called from an event.
 */
int isn_reactor_pass(const isn_reactor_tasklet_t tasklet, void* arg);

/** Queue an tasklet
 * \returns index in the queue >=0 on success, -1 if queue is full
 */
static inline int isn_reactor_queue(const isn_reactor_tasklet_t tasklet, void* arg) {
    return isn_reactor_call_at(tasklet, NULL, arg, ISN_CLOCK_NOW);
}

/** Queue a timed tasklet
 * \returns index in the queue >=0 on success, -1 if queue is full
 */
static inline int isn_reactor_queue_at(const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed) {
    return isn_reactor_call_at(tasklet, NULL, arg, timed);
}

/** \returns next free mutex bit, or 0 if none is available */
isn_reactor_mutex_t isn_reactor_getmutex();

/** Assign a mutex to an tasklet in the time of posting it
 *
 * Currently we have 4 mutex groups (bits), 1, 2, 4 and 8.
 * It can be easily extend to 8 and using lower 6 bits for indexing.
 *
 * Example:
 *   isn_reactor_mutexqueue(mytasklet, NULL, 2);
 */
int isn_reactor_mutexqueue(const isn_reactor_tasklet_t tasklet, void* arg, isn_reactor_mutex_t mutex_bits);

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
int isn_reactor_change_timed(int index, const isn_reactor_tasklet_t tasklet, const void* arg, isn_clock_counter_t newtime);

/** Modify time for reoccuring (self-triggered) event, from the event.
 *  So the function modifies the time of the active event and only has effect
 *  if event returns with a pointer to itself.
 */
int isn_reactor_change_timed_self(isn_clock_counter_t newtime);

/** Drop specific tasklet
 * \returns 0 if no-longer in queue or invalid index, and 1 when modified successfully
 */
int isn_reactor_drop(int index, const isn_reactor_tasklet_t tasklet, const void* arg);

/** Drop tasklets from queue of given tasklet and arg
 *  \returns number of removed tasklets
 */
int isn_reactor_dropall(const isn_reactor_tasklet_t tasklet, const void* arg);

/** Returns number of pending events with higher and same priorities
 *
 * This can be useful to pre-empt i.e. lower priority task that may take longer
 * and give preceedence to other events. Calling this function only has sense
 * from the event themselves, as it returns the number the event queue increased
 * while current event is being executed. Practical uses are i.e. math post-processing
 *
 * Implementation may due to optimizations not report the exact number in the queue,
 * however the required condition is that 0 is always reported corretly, that there
 * are no higher priority events and also no other events, than this, in the same
 * priority queue.
 *
 * \returns number of active events waiting in the queue of the same priority, where
 *   0 means that the event from which this function is called is the last pending
 *   event to be executed. Negative value represents invalid result, i.e. when executed
 *   outside the event.
 */
int isn_reactor_is_last();

/** Execute one tasklet only.
 * \returns Non-zero if any tasklet has been executed
 */
int isn_reactor_step(void);

/** Execute all pending tasklets
 * \returns time to next execution referred to a isn clock timer
 */
isn_clock_counter_t isn_reactor_run(void);

/** Executes all tasklets from foreign queues (channels), NULL terminated, followed by local pending tasklets */
isn_clock_counter_t isn_reactor_runall(isn_tasklet_queue_t *queue, ...);

/** Self-test, performs basic and mutex queues check
 *  Side effect, it uses one mutex and does not free it.
 * \returns 0 on success, or negative value showing progress
 */
int isn_reactor_selftest();

/** Initialize reactor and provide queue buffer */
void isn_reactor_init(isn_tasklet_entry_t *tasklet_queue, size_t queue_size);

#endif
