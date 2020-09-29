/** \file
 *  \brief Isotel Sensor Network Reactor Implementation
 *  \author <uros@isotel.eu>, <stanislav@isotel.eu>,
 *
 * Implementation of a very reduced and simple reactor,
 * supporting qeued tasklets offering timed execution,
 * mutex locking, and return to callers.
 *
 * Currently supported and tested on Cypress PSoC5 only.
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

#include <project.h>                // Currently supported/tested on PSoC5 only
#include "isn_reactor.h"

#if(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC4)
# error "PSoC4 has not been yet verified and supported"
#elif(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC5)
# warning "Experimental optimizations in mutexes and queue linking"
# define MUTEX_SHIFT                20
# define MUTEX_COUNT                4
# define MUTEX_MASK                 0xF
# define FUNC_ADDR_MASK             0x0003FFFF
# define INDEX_SHIFT                24
# define QUEUE_FUNC_ADDR(i)         (void *)((uint32_t)(queue_table[i].tasklet) & FUNC_ADDR_MASK)
#elif(CYDEV_CHIP_FAMILY_USED == CYDEV_CHIP_FAMILY_PSOC6)
# warning "Experimental support of reactor"
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

isn_reactor_time_t _isn_reactor_active_timestamp;
const volatile isn_reactor_time_t* _isn_reactor_timer;
isn_reactor_time_t isn_reactor_timer_trigger;

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

int isn_reactor_call_at(const isn_reactor_tasklet_t tasklet, const isn_reactor_caller_t caller, const void* arg, isn_reactor_time_t time) {
    critical_section_state_t state = critical_section_enter();
    if (QUEUE_NEXT(queue_free) == queue_len || tasklet == NULL) {
        critical_section_exit(state);
        return -1;
    }
    queue_table[queue_free].tasklet  = (void*)((uint32_t)queue_table[queue_free].tasklet | ((uint32_t)tasklet & (FUNC_ADDR_MASK | (MUTEX_MASK<<MUTEX_SHIFT)) ));
    queue_table[queue_free].caller   = caller;
    queue_table[queue_free].arg      = arg;
    queue_table[queue_free].time     = time;
    queue_changed = 1;   // we have at least one to work-on
    int queue_index = queue_free;
    ++isn_tasklet_queue_size;

    queue_free = QUEUE_NEXT(queue_free);
    critical_section_exit(state);
    return queue_index;
}

/** At the moment we only have 4 muxes */
isn_reactor_mutex_t isn_reactor_getmutex() {
    static uint32_t muxes = 0;
    if (muxes > MUTEX_COUNT) return 0; else return (1<<muxes++)<<MUTEX_SHIFT;
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

int isn_reactor_mutexqueue(const isn_reactor_tasklet_t tasklet, const void* arg, isn_reactor_mutex_t mutex_bits) {
    return isn_reactor_queue( (void *)(((uint32_t)tasklet & FUNC_ADDR_MASK) | mutex_bits), arg);
}

int isn_reactor_isvalid(int index, const isn_reactor_tasklet_t tasklet, const void* arg) {
    if (index >= queue_len || index < 0) return 0;
    return (QUEUE_FUNC_ADDR(index) == tasklet && queue_table[index].arg == arg) ? 1 : 0;
}

int isn_reactor_change_timed(int index, const isn_reactor_tasklet_t tasklet, const void* arg, isn_reactor_time_t newtime) {
    critical_section_state_t state = critical_section_enter();
    int retval = isn_reactor_isvalid(index, tasklet, arg);
    if (retval) {
        queue_table[index].time = newtime;
        queue_changed = 1;
    }
    critical_section_exit(state);
    return retval;
}

int isn_reactor_change_timed_self(isn_reactor_time_t newtime) {
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
    if (retval) queue_table[index].tasklet = NULL;
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
 *
 *  \todo Handle caller return codes to support re-triggering actions
 */
int isn_reactor_step(void) {
    int executed = 0;
    int32_t next_time_to_exec = 0x0FFFFFF;

    if (queue_changed || (int32_t)(isn_reactor_timer_trigger - *_isn_reactor_timer) <= 0) {
        uint8_t i, j;
        queue_changed = 0;   // assume we are executing the last, if ISR meanwhile occurs it will only set it to number of queue size

        for (i=0, j=QUEUE_NEXT(0); QUEUE_FUNC_VALID(j); ) {
            if ( !(QUEUE_MUTEX(j) & queue_mutex_locked_bits) ) {
                int32_t time_to_exec = (int32_t)(QUEUE_TIME(j) - *_isn_reactor_timer);
                if (time_to_exec <= 0) {
                    isn_reactor_tasklet_t tasklet = QUEUE_FUNC_ADDR(j);
                    _isn_reactor_active_timestamp = queue_table[j].time;
                    self_index                    = j;

                    executed++;
                    const void *retval = NULL;
                    if (tasklet) {
                        retval = tasklet( (const void *)queue_table[j].arg );
                        if (retval == (const void *)tasklet) continue; // returning self means retrigger the event
                        if ( queue_table[j].caller ) {
                            queue_table[j].caller( tasklet, (const void *)queue_table[j].arg, retval );
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
            i = j;
            j = QUEUE_NEXT(j);
        }
        isn_reactor_timer_trigger = *_isn_reactor_timer + next_time_to_exec;
    }
    self_index = -1;
    return executed;
}

isn_reactor_time_t isn_reactor_run(void) {
    while( isn_reactor_step() );
    return isn_reactor_timer_trigger;
}

int isn_reactor_selftest() {
    static int count = 0;
    isn_reactor_mutex_t mux = isn_reactor_getmutex();

    void *count_event(const void *arg) {
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

void isn_reactor_init(isn_tasklet_entry_t *tasklet_queue, size_t queue_size, const volatile const isn_reactor_time_t* timer) {
    _isn_reactor_timer = timer;
    queue_table = tasklet_queue;
    queue_len   = queue_size;
    queue_free  = 1; // leave 0th cell for algo simplification as well as last.
    queue_changed = 0;
    queue_mutex_locked_bits = 0;
    isn_tasklet_queue_size = 0;
    isn_tasklet_queue_max = 0;
    for (uint8_t i=0; i<queue_len; i++) QUEUE_LINKANDCLEAR(i, i+1);
}
