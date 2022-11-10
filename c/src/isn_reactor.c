/** \file
 *  \brief Isotel Sensor Network Reactor Implementation
 *  \author <uros@isotel.org>
 */
/**
 * \ingroup GR_ISN
 * \cond Implementation
 * \addtogroup GR_ISN_Reactor

 * Implementation of a very reduced and simple reactor,
 * supporting qeued tasklets offering timed execution,
 * mutex locking, and return to callers.
 *
 * Currently supported and tested on Cypress PSoC5 and PSoC6.
 */
/*
    QUEUE: Instead of a FIFO single linked list is used to be able to implement simple mutex locking

    Simple small ARM solution uses 4 bits to encode mutexes and assumes:
        pointers are in the range: 0x0000 0000 – 0x0003 FFFF (CPU: PSoC5)
    One byte is used for linking

    QUEUE Operations:

    List layout:
        head -> [x] -> [x] ---> [ ] -> [ ] .. -> [ ] -> END
        free --------------------|

    Initial cond:
        head -> [ ] -> [ ] ... -> END
        free ----|

    End cond:
        head -> [x] -> [ ] -> END (leave last free empty as terminator for alg simpl.)
        free -----------|

    Adding:     fill the last free and move last's index forward until last item has ref END, leave that empty!
        head -> [x] -> [ ] ... -> END
        free ----|----->|

    Exec:       head to first empty and execute, go forward until you find empty one
        head -> [x] -> .. -> [ ] -> END
        free -----------------|

    Freeing:    make item empty and modify last free ref to it and over-take last reference
        head -> .. -> [ ] -> [*] -> [ ] -> ... -> END
        free ----------|    (ins)

        modify head link if it is the first
        modify previous item list if it is in the middle
        head may also be the 0-th entry to simplify algo.
        Easy to upgrade with priorities.
*/
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2021, Isotel, http://isotel.org
 */

#include <project.h>                // Currently supported/tested on PSoC5 only
#include <stdarg.h>
#include "isn_reactor.h"

#if(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC4)
# error "PSoC4 has not been yet verified and supported"
#elif(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)
# define MUTEX_SHIFT                20
# define MUTEX_COUNT                4
# define MUTEX_MASK                 0xF
# define FUNC_ADDR_MASK             0x0003FFFF
# define INDEX_SHIFT                24
# define QUEUE_FUNC_ADDR(i)         (void *)((uint32_t)(queue_table[i].tasklet) & FUNC_ADDR_MASK)
#elif(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)
# define MUTEX_SHIFT                28
# define MUTEX_COUNT                4
# define MUTEX_MASK                 0xF
# define FUNC_ADDR_MASK             0x000FFFFF
# define INDEX_SHIFT                20
# define QUEUE_FUNC_ADDR(i)         (void *)(((uint32_t)(queue_table[i].tasklet) & FUNC_ADDR_MASK) | 0x10000000)
#else
# error "Not (yet) supported platform"  0x10050000
#endif

#define QUEUE_LINK(i,j)             queue_table[i].tasklet = (void *)( ((uint32_t)queue_table[i].tasklet & ~(0xFF << INDEX_SHIFT) ) | ((j)<<INDEX_SHIFT) )
#define QUEUE_LINKANDCLEAR(i, j)    queue_table[i].tasklet = (void *)((j) << INDEX_SHIFT)
#define QUEUE_NEXT(i)               (((uint32_t)(queue_table[i].tasklet) >> INDEX_SHIFT) & 0xFF)
#define QUEUE_MUTEX(i)              ((uint32_t)(queue_table[i].tasklet) & (MUTEX_MASK << MUTEX_SHIFT))
#define QUEUE_FUNC_VALID(i)         (void *)((uint32_t)(queue_table[i].tasklet) & FUNC_ADDR_MASK)
#define QUEUE_TIME(i)               queue_table[i].time

/**\{ */

isn_clock_counter_t _isn_reactor_active_timestamp;
isn_clock_counter_t isn_reactor_timer_trigger;

static isn_tasklet_entry_t *queue_table;
static size_t queue_len;
static volatile isn_reactor_mutex_t queue_mutex_locked_bits = 0;
static volatile uint8_t queue_free = 0;
static volatile uint32_t queue_changed = 0; ///< Non-zero if event queue loop should re-run

uint32_t isn_tasklet_queue_size = 0;
uint32_t isn_tasklet_queue_max = 0;
static int self_index = -1;

typedef uint8_t critical_section_state_t;

static inline critical_section_state_t critical_section_enter() {
	return CyEnterCriticalSection();
}

static inline void critical_section_exit(critical_section_state_t s) {
	CyExitCriticalSection(s);
}

static inline void atomic_set_bits(volatile uint32_t* addr, uint32_t mask) {
    uint8_t s = CyEnterCriticalSection();
    *addr |= mask;
    CyExitCriticalSection(s);
}

static inline void atomic_clear_bits(volatile uint32_t* addr, uint32_t mask) {
    uint8_t s = CyEnterCriticalSection();
    *addr &= ~mask;
    CyExitCriticalSection(s);
}

void isn_reactor_initchannel(isn_tasklet_queue_t *queue, isn_tasklet_entry_t* fifobuf, size_t size_mask) {
    if (fifobuf && size_mask) {
        queue->fifo = fifobuf;
        queue->size_mask = size_mask;
        queue->rdi = queue->wri = 0;
        queue->wakeup = NULL;
    }
}

int isn_reactor_channel_at(isn_tasklet_queue_t *queue, const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed) {
    size_t next = (queue->wri+1) & queue->size_mask;
    if (queue->rdi != next) {
        isn_tasklet_entry_t *e = &queue->fifo[queue->wri];
        e->tasklet = tasklet;
        e->caller = NULL;
        e->caller_queue = NULL;
        e->arg = arg;
        e->time = timed;
        __DSB();
        queue->wri = next;
        if (queue->wakeup) queue->wakeup();
        return 0;
    }
    return -1;
}

int isn_reactor_channel_call_at(isn_tasklet_queue_t *queue, const isn_reactor_tasklet_t tasklet,
                                isn_tasklet_queue_t *caller_queue, const isn_reactor_tasklet_t caller,
                                void* arg, isn_clock_counter_t timed) {
    size_t next = (queue->wri+1) & queue->size_mask;
    if (queue->rdi != next) {
        isn_tasklet_entry_t *e = &queue->fifo[queue->wri];
        e->tasklet = tasklet;
        e->caller = caller;
        e->caller_queue = caller_queue;
        e->arg = arg;
        e->time = timed;
        __DSB();
        queue->wri = next;
        if (queue->wakeup) queue->wakeup();
        return 0;
    }
    return -1;
}

int isn_reactor_channel_return(isn_tasklet_queue_t *queue, const isn_reactor_tasklet_t caller, void* arg) {
    size_t next = (queue->wri+1) & queue->size_mask;
    if (queue->rdi != next) {
        isn_tasklet_entry_t *e = &queue->fifo[queue->wri];
        e->tasklet = NULL;
        e->caller = caller;
        e->caller_queue = NULL;
        e->arg = arg;
        e->time = ISN_CLOCK_NOW;
        __DSB();
        queue->wri = next;
        if (queue->wakeup) queue->wakeup();
        return 0;
    }
    return -1;
}

int isn_reactor_pass(const isn_reactor_tasklet_t tasklet, void* arg) {
    if (self_index < 0) return -1;

    critical_section_state_t state = critical_section_enter();
    if (QUEUE_NEXT(queue_free) == queue_len || tasklet == NULL) {
        critical_section_exit(state);
        return -1;
    }
    queue_table[queue_free].tasklet  = (void*)((uint32_t)queue_table[queue_free].tasklet | ((uint32_t)tasklet & (FUNC_ADDR_MASK | (MUTEX_MASK<<MUTEX_SHIFT)) ));
    queue_table[queue_free].caller   = queue_table[self_index].caller;
    queue_table[self_index].caller   = NULL;
    queue_table[queue_free].caller_queue = queue_table[self_index].caller_queue;
    queue_table[self_index].caller_queue = NULL;
    queue_table[queue_free].arg      = arg;
    queue_table[queue_free].time     = ISN_CLOCK_NOW;
    queue_changed = 1;   // we have at least one to work-on
    int queue_index = queue_free;
    ++isn_tasklet_queue_size;

    queue_free = QUEUE_NEXT(queue_free);
    critical_section_exit(state);
    return queue_index;
}

int isn_reactor_call_at(const isn_reactor_tasklet_t tasklet, const isn_reactor_tasklet_t caller, void* arg, isn_clock_counter_t time) {
    critical_section_state_t state = critical_section_enter();
    if (QUEUE_NEXT(queue_free) == queue_len || tasklet == NULL) {
        critical_section_exit(state);
        return -1;
    }
    queue_table[queue_free].tasklet  = (void*)((uint32_t)queue_table[queue_free].tasklet | ((uint32_t)tasklet & (FUNC_ADDR_MASK | (MUTEX_MASK<<MUTEX_SHIFT)) ));
    queue_table[queue_free].caller   = caller;
    queue_table[queue_free].caller_queue = NULL;
    queue_table[queue_free].arg      = arg;
    queue_table[queue_free].time     = time;
    queue_changed = 1;   // we have at least one to work-on
    int queue_index = queue_free;
    ++isn_tasklet_queue_size;

    queue_free = QUEUE_NEXT(queue_free);
    critical_section_exit(state);
    return queue_index;
}

static int isn_reactor_callx_at(const isn_reactor_tasklet_t tasklet, isn_tasklet_queue_t *caller_queue, const isn_reactor_tasklet_t caller, void* arg, isn_clock_counter_t time) {
    critical_section_state_t state = critical_section_enter();
    if (QUEUE_NEXT(queue_free) == queue_len || tasklet == NULL) {
        critical_section_exit(state);
        return -1;
    }
    queue_table[queue_free].tasklet  = (void*)((uint32_t)queue_table[queue_free].tasklet | ((uint32_t)tasklet & (FUNC_ADDR_MASK | (MUTEX_MASK<<MUTEX_SHIFT)) ));
    queue_table[queue_free].caller   = caller;
    queue_table[queue_free].caller_queue = caller_queue;
    queue_table[queue_free].arg      = arg;
    queue_table[queue_free].time     = time;
    queue_changed = 1;   // we have at least one to work-on
    int queue_index = queue_free;
    ++isn_tasklet_queue_size;

    queue_free = QUEUE_NEXT(queue_free);
    critical_section_exit(state);
    return queue_index;
}

int isn_reactor_userqueue(const isn_reactor_tasklet_t tasklet, void* arg, isn_clock_counter_t timed, isn_reactor_mutex_t mutex_bits) {
    return isn_reactor_call_at((void *)(((uint32_t)tasklet & FUNC_ADDR_MASK) | mutex_bits), NULL, arg, timed);
}

/** At the moment we only have 4 muxes */
isn_reactor_mutex_t isn_reactor_getmutex() {
    static uint32_t muxes = 0;
    return muxes > MUTEX_COUNT ? 0 : (1<<muxes++) << MUTEX_SHIFT;
}

int isn_reactor_mutex_lock(isn_reactor_mutex_t mutex_bits) {
    isn_reactor_mutex_t old_locks = queue_mutex_locked_bits;
    atomic_set_bits(&queue_mutex_locked_bits, mutex_bits);
    return queue_mutex_locked_bits == old_locks;
}

int isn_reactor_mutex_unlock(isn_reactor_mutex_t mutex_bits) {
    isn_reactor_mutex_t old_locks = queue_mutex_locked_bits;
    atomic_clear_bits(&queue_mutex_locked_bits, mutex_bits);
    if (queue_mutex_locked_bits == old_locks) return 1;
    queue_changed = 1;
    return 0;
}

int isn_reactor_mutex_is_locked(isn_reactor_mutex_t mutex_bits) {
    return (queue_mutex_locked_bits & mutex_bits) != 0;
}

int isn_reactor_mutexqueue(const isn_reactor_tasklet_t tasklet, void* arg, isn_reactor_mutex_t mutex_bits) {
    return isn_reactor_queue( (void *)(((uint32_t)tasklet & FUNC_ADDR_MASK) | mutex_bits), arg);
}

int isn_reactor_isvalid(int index, const isn_reactor_tasklet_t tasklet, const void* arg) {
    if (index >= queue_len || index < 0) return 0;
    return (QUEUE_FUNC_ADDR(index) == tasklet && queue_table[index].arg == arg) ? 1 : 0;
}

int isn_reactor_change_timed(int index, const isn_reactor_tasklet_t tasklet, const void* arg, isn_clock_counter_t newtime) {
    critical_section_state_t state = critical_section_enter();
    int retval = isn_reactor_isvalid(index, tasklet, arg);
    if (retval) {
        queue_table[index].time = newtime;
        queue_changed = 1;
    }
    critical_section_exit(state);
    return retval;
}

int isn_reactor_change_timed_self(isn_clock_counter_t newtime) {
    if (self_index >= 0) {
        queue_table[self_index].time = newtime;
        queue_changed = 1;
        return 0;
    }
    return -1;
}

int isn_reactor_drop(int index, const isn_reactor_tasklet_t tasklet, const void* arg) {
    critical_section_state_t state = critical_section_enter();
    int retval = isn_reactor_isvalid(index, tasklet, arg);
    if (retval && index != self_index) queue_table[index].tasklet = NULL;
    critical_section_exit(state);
    return retval;
}

#ifdef REQUIRES_BUGFIX
int isn_reactor_dropall(const isn_reactor_tasklet_t tasklet, const void* arg) {
    int removed = 0;
    uint8_t i,j;
    for (i=0, j=QUEUE_NEXT(0); QUEUE_FUNC_VALID(j); ) {
        if (tasklet == QUEUE_FUNC_ADDR(j) && arg == (const void *)queue_table[j].arg) {
            QUEUE_LINK(i, QUEUE_NEXT(j));

            critical_section_state_t state = critical_section_enter();
            QUEUE_LINKANDCLEAR(j, QUEUE_NEXT(queue_free));
            QUEUE_LINK(queue_free, j);
            queue_changed = --isn_tasklet_queue_size; // avoid additional looping if it is last
            critical_section_exit(state);
            j = i;
            removed++;
        }
        i = j;
        j = QUEUE_NEXT(j);
    }
    return removed;
}
#endif

#define MAX_SLEEP_TIME    0x0FFFFFFF    // \todo Consider appropriate max time according to isn_clock.c constraints, derive macro from there

/** Execute first available, skip mutexed and delayed.
 *
 *  Due to the nature of list operation, all mutex locked tasklet will concentrate at the beginning
 *  of the list, that needs to be skipped on each iteration.
 *
 *  \todo Implement simplest speed optimization (via mutex_changed variable)
 *   - remember last mutexed event and continue from this head instead beginning, to reduce for looping
 *   - on any mutex release change, reset head to 0
 *
 *  \todo 2nd optimization is to perform timed bubble sort while looping thru the list
 */
int isn_reactor_step(void) {
    int executed = 0;
    int32_t next_time_to_exec = MAX_SLEEP_TIME;

    if (queue_changed || (int32_t)(isn_reactor_timer_trigger - ISN_CLOCK_NOW) <= 0) {
        uint8_t i, j;
        queue_changed = 0;   // assume we are executing the last, if ISR meanwhile occurs it will only set it to number of queue size

        for (i=0, j=QUEUE_NEXT(0); QUEUE_FUNC_VALID(j); ) {
            if ( !(QUEUE_MUTEX(j) & queue_mutex_locked_bits) ) {
                int32_t time_to_exec = isn_clock_remains(QUEUE_TIME(j));
                if (time_to_exec <= 0) {
                    isn_reactor_tasklet_t tasklet = QUEUE_FUNC_ADDR(j);
                    _isn_reactor_active_timestamp = queue_table[j].time;
                    self_index                    = j;

                    executed++;
                    void *retval = NULL;
                    if (tasklet) {
                        retval = tasklet( queue_table[j].arg );

                        // returning self means retrigger the event, but in next pass to avoid forever looping/stalling
                        // another possibility to retrigger the event is to set event time in advance, so even if
                        // Dual feature is useful, i.e. interrupt may keep prolonging the time of a valid event, which
                        // valid event has just being executed in the background. Interrupt would see it valid, so
                        // it may prolong its time, while it would not create a new one. And the event which is fetching
                        // the bytes is for sure retriggered.
                        time_to_exec = isn_clock_remains(QUEUE_TIME(j));
                        if (retval == (const void *)tasklet || time_to_exec >= 0) {
                            if (time_to_exec < 0) {
                                queue_table[j].time = isn_clock_now();  // theoretically prevents overflow of constantly re-occuring event which would get blocked after clock elapsed 1/2 of the cycle
                                next_time_to_exec = 0;
                            }
                            else next_time_to_exec = time_to_exec;
                            goto do_next_event;
                        }
                        if ( queue_table[j].caller ) {
                            if (queue_table[j].caller_queue) {
                                isn_reactor_channel_return( queue_table[j].caller_queue, queue_table[j].caller, retval);
                            }
                            else queue_table[j].caller( retval );
                        }
                    }
                    QUEUE_LINK(i, QUEUE_NEXT(j));

                    critical_section_state_t state = critical_section_enter();
                    QUEUE_LINKANDCLEAR(j, QUEUE_NEXT(queue_free));
                    QUEUE_LINK(queue_free, j);

                    if (isn_tasklet_queue_size > isn_tasklet_queue_max) isn_tasklet_queue_max = isn_tasklet_queue_size;
                    queue_changed = --isn_tasklet_queue_size; // avoid additional looping if it is last
                    critical_section_exit(state);
                    j = i;
                }
                else if (time_to_exec < next_time_to_exec) {
                    next_time_to_exec = time_to_exec;
                }
            }
do_next_event:
            i = j;
            j = QUEUE_NEXT(j);
        }
        isn_reactor_timer_trigger = ISN_CLOCK_NOW + next_time_to_exec;
    }
    self_index = -1;
    return executed;
}

isn_clock_counter_t isn_reactor_run(void) {
    while( isn_reactor_step() );
    return isn_reactor_timer_trigger;
}

isn_clock_counter_t isn_reactor_runall(isn_tasklet_queue_t *queue, ...) {
    va_list va;
    va_start(va, queue);
    while(queue) {
        while(queue->rdi != queue->wri) {
            isn_tasklet_entry_t *e = &queue->fifo[queue->rdi];
            /*
                If tasklet is given it is a normal cross-cpu call, we spawn it into the queue
                If tasklet is NULL but caller is given it is a return feedback call; currently tasklet for cross-cpu
                is marked as NULL (TBD if really needed)
            */
            if (e->tasklet) isn_reactor_callx_at( e->tasklet, e->caller_queue, e->caller, e->arg, e->time ); else
            if (e->caller)  e->caller( e->arg );
            queue->rdi = (queue->rdi+1) & queue->size_mask;
        }
        queue = va_arg(va, isn_tasklet_queue_t *);
    }
    va_end(va);

    // local events are called last, as above function may either post timed events
    // or actual events may change local events, which could impact on the time to
    // trigger.
    while( isn_reactor_step() );

    return isn_reactor_timer_trigger;
}

int isn_reactor_selftest() {
    static int count = 0;
    isn_reactor_mutex_t mux = isn_reactor_getmutex();

    void *count_event(void *arg) {
        count++;
        return NULL;
    }

    isn_reactor_queue(count_event, NULL);
    isn_reactor_run();
    if (count != 1) return -1;
    isn_reactor_mutex_lock( mux );
    isn_reactor_mutexqueue(count_event, NULL, mux);
    isn_reactor_run();
    if (count != 1) return -2;
    isn_reactor_mutex_unlock( mux );
    isn_reactor_run();
    if (count != 2) return -3;
    return 0;
}

void isn_reactor_init(isn_tasklet_entry_t *tasklet_queue, size_t queue_size) {
    ASSERT(tasklet_queue);
    queue_table = tasklet_queue;
    queue_len   = queue_size;
    queue_free  = 1; // leave 0th cell for algo simplification as well as last.
    queue_changed = 0;
    queue_mutex_locked_bits = 0;
    isn_tasklet_queue_size = 0;
    isn_tasklet_queue_max = 0;
    for (uint8_t i=0; i<queue_len; i++) QUEUE_LINKANDCLEAR(i, i+1);
}

/** \} \endcond */
