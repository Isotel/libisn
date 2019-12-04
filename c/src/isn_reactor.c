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

    Simple small ARM solution uses upper 4 bits to encode mutexes and assumes:
        pointers are in the range: 0x0000 0000 – 0x0003 FFFF (CPU: PSoC5)
    Then:
     - upper byte is used for linking
     - next nibble is used for 4 mutex groups

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

#define QUEUE_LINK(i,j)             queue_table[i].tasklet = (void *)(((uint32_t)(queue_table[i].tasklet) & 0x00FFFFFF) | ((j)<<24))
#define QUEUE_LINKANDCLEAR(i, j)    queue_table[i].tasklet = (void *)((j)<<24)
#define QUEUE_NEXT(i)               (((uint32_t)(queue_table[i].tasklet) & 0xFF000000) >> 24)
#define QUEUE_FUNC_ADDR(i)          (void *)((uint32_t)(queue_table[i].tasklet) & 0x0003FFFF)
#define QUEUE_MUTEX(i)              ((uint32_t)(queue_table[i].tasklet) & 0x00FC0000)
#define QUEUE_TIME(i)               queue_table[i].time

isn_reactor_time_t isn_reactor_active_timestamp;
const volatile isn_reactor_time_t* _isn_reactor_timer;
isn_reactor_time_t isn_reactor_timer_trigger;

static isn_tasklet_entry_t *queue_table;
static size_t queue_len;
static volatile uint32_t queue_mutex_locked_bits = 0;
static volatile uint32_t mutex_changed = 0;
static volatile uint8_t queue_free = 0;
static volatile uint32_t queue_changed = 0; ///< Non-zero if event queue loop should re-run

uint32_t isn_tasklet_queue_size;
uint32_t isn_tasklet_queue_max;

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
    queue_table[queue_free].tasklet  = (void*)((uint32_t)queue_table[queue_free].tasklet | (uint32_t)tasklet);
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
uint32_t isn_reactor_getmutex() {
    static uint32_t muxes = 0;
    if (muxes > 4) return 0; else return (1<<muxes++);
}

void isn_reactor_mutex_lock(uint32_t mutex_bits) {
    atomic_set_bits(&queue_mutex_locked_bits, (mutex_bits & 0xF)<<20);
}

void isn_reactor_mutex_unlock(uint32_t mutex_bits) {
    atomic_clear_bits(&queue_mutex_locked_bits, (mutex_bits & 0xF)<<20);
    mutex_changed = 1;
}

uint32_t isn_reactor_mutex_is_locked(uint32_t mutex_bits) {
    return queue_mutex_locked_bits & ((mutex_bits & 0xF)<<20);
}

int isn_reactor_mutexqueue(const isn_reactor_tasklet_t tasklet, const void* arg, uint32_t mutex_bits) {
    return isn_reactor_queue( (void *)((uint32_t)tasklet | (mutex_bits & 0xF)<<20), arg);
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

int isn_reactor_dropall(const isn_reactor_tasklet_t tasklet, const void* arg) {
    int removed = 0;
    uint8_t i,j;
    for (i=0, j=QUEUE_NEXT(0); QUEUE_FUNC_ADDR(j); ) {
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

/** Execute first available, skip mutexed and delayed.
 *
 *  Due to the nature of list operation, all mutex locked tasklet will concentrate at the beginning
 *  of the list, that needs to be skipped on each iteration.
 *
 *  \todo Implement simplest speed optimization (see: mutex_changed variable)
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

        for (i=0, j=QUEUE_NEXT(0); QUEUE_FUNC_ADDR(j); ) {
            if ( !(QUEUE_MUTEX(j) & queue_mutex_locked_bits) ) {
                int32_t time_to_exec = (int32_t)(QUEUE_TIME(j) - *_isn_reactor_timer);
                if (time_to_exec <= 0) {
                    isn_reactor_tasklet_t tasklet = QUEUE_FUNC_ADDR(j);
                    isn_reactor_active_timestamp = queue_table[j].time;
                    const void *retval = tasklet( (const void *)queue_table[j].arg );
                    if  (queue_table[j].caller ) {
                        queue_table[j].caller( tasklet, (const void *)queue_table[j].arg, retval );
                    }
                    QUEUE_LINK(i, QUEUE_NEXT(j));

                    critical_section_state_t state = critical_section_enter();
                    QUEUE_LINKANDCLEAR(j, QUEUE_NEXT(queue_free));
                    QUEUE_LINK(queue_free, j);

                    if (isn_tasklet_queue_size > isn_tasklet_queue_max) isn_tasklet_queue_max = isn_tasklet_queue_size;
                    queue_changed = --isn_tasklet_queue_size; // avoid additional looping if it is last
                    critical_section_exit(state);
                    executed++;
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
    return executed;
}

isn_reactor_time_t isn_reactor_run(void) {
    while( isn_reactor_step() );
    return isn_reactor_timer_trigger;
}

void isn_reactor_init(isn_tasklet_entry_t *tasklet_queue, size_t queue_size, const volatile isn_reactor_time_t* timer) {
    _isn_reactor_timer = timer;
    queue_table = tasklet_queue;
    queue_len   = queue_size;
    queue_free  = 1; // leave 0th cell for algo simplification as well as last.
    for (uint8_t i=0; i<queue_len; i++) QUEUE_LINKANDCLEAR(i, i+1);
}
