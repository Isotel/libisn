/** \file
 *  \brief ISN Message Layer Implementation
 *  \author Uros Platise <uros@isotel.org>
 *  \see isn_msg.h
 */
/**
 * \ingroup GR_ISN
 * \cond Implementation
 * \addtogroup GR_ISN_Message
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2021, Isotel, http://isotel.org
 */

#include <string.h>
#include <stdlib.h>
#include "assert.h"
#include "isn_msg.h"

isn_message_t *isn_msg_self;

static isn_logger_level_t isn_logger_level = ISN_LOGGER_LOG_LEVEL_FATAL;

/* Satisfy compilation even without reactor */
int isn_reactor_mutex_ignored(isn_reactor_mutex_t mutex_bits) {return 0;}
int isn_reactor_mutex_lock(isn_reactor_mutex_t mutex_bits) __attribute__ ((weak, alias("isn_reactor_mutex_ignored")));
int isn_reactor_mutex_unlock(isn_reactor_mutex_t mutex_bits) __attribute__ ((weak, alias("isn_reactor_mutex_ignored")));

/**\{ */

static int send_packet(isn_message_t *obj, uint8_t msgflags, const void* data, isn_msg_size_t size) {
    void *dest = NULL;
    int xsize = size + 2;

    if (obj->parent_driver->getsendbuf(obj->parent_driver, &dest, xsize, (isn_layer_t *)obj) == xsize) {
        uint8_t *buf = dest;
        *buf     = ISN_PROTO_MSG;
        *(buf+1) = msgflags;
        isn_memcpy(buf+2, data, size);
        obj->parent_driver->send(obj->parent_driver, buf, xsize);
        obj->drv.stats.tx_packets++;
        obj->drv.stats.tx_counter+=size;
        return size;
    }
    else if (dest) {
        obj->parent_driver->free(obj->parent_driver, dest);
    }
    obj->drv.stats.tx_dropped++;
    //obj->parent_driver->free(obj->parent_driver, dest);   // we're ok to free NULL to simplify code
    return 0;
}

/** Send next message in a round-robin way */
static int isn_msg_sendnext(isn_message_t *obj) {
    isn_msg_table_t* picked = NULL;
    uint8_t* data = NULL;
    obj->active = 0;

	for (uint8_t i = 0; i < obj->isn_msg_table_size; obj->msgnum++, i++) {
		if (obj->msgnum >= obj->isn_msg_table_size) obj->msgnum = 0;
		if (obj->isn_msg_table[obj->msgnum].priority > 0) {
            obj->active++;

            // If it is locked in a query wait state then we want to unlock it (proceed) only if data is provided
            // Even if locked, keep through other messages to free input receive buffer
            if ( (obj->isn_msg_table[obj->msgnum].priority != __ISN_MSG_PRI_QUERY_WAIT && !obj->lock) ||
                     obj->msgnum == obj->isn_msg_received_msgnum) {
                picked = &obj->isn_msg_table[obj->msgnum];
                break;
            }
		}
	}
    // Reset availability of tx buffer for given argument size, indeed we should also test
    // for desc loading, however this goes typically one after another
    if (picked) {
        /*
         * Find out requred packet size to avoid tx drops
         * WAIT_ARGS require full picked->size as it is an answer, as all of the messages below that priority
         * The extra 2 is for the protocol, see send_packet()
         */
        size_t required_size;
             if (picked->priority >= ISN_MSG_PRI_DESCRIPTIONLOW) required_size = strlen(picked->desc) + 2;
        else if (picked->priority == ISN_MSG_PRI_QUERY_ARGS)     required_size = 2;
        else                                                     required_size = picked->size + 2;

        if (obj->parent_driver->getsendbuf(obj->parent_driver, NULL, required_size, (isn_layer_t *)obj) == required_size) {
            isn_msg_self = obj;

            // Set and release locks
            if (obj->isn_msg_received_msgnum == obj->lock) obj->lock = 0;
            else if (picked->priority == ISN_MGG_PRI_UPDATE_ARGS
#if CONFIG_ISN_MSG_SINGLE_QUERY > 0
                  || picked->priority == ISN_MSG_PRI_QUERY_ARGS
#endif
                                                                 ) {
                obj->lock = obj->msgnum;
                obj->resend_timer = 0;
            }

            if (picked->priority >= ISN_MSG_PRI_DESCRIPTIONLOW) {
                send_packet(obj, (uint8_t)0x80 | obj->msgnum, picked->desc, required_size - 2 /*header*/);
                picked->priority = (obj->msgnum == obj->isn_msg_received_msgnum) ? ISN_MSG_PRI_HIGHEST : ISN_MSG_PRI_LOW;
            }
    #ifdef TODO_CLARIFY_WITH_IDM
            // a message without args cannot be sent as args, but only desc (first if)
            else if (picked->size == 0) {
                picked->priority = ISN_MSG_PRI_CLEAR;
                obj->drv.stats.tx_dropped++;
            }
    #endif
            // Note that the message we're asking for is just arriving, and for messages without
            // any handler, we reply back with query, but we do not block the message.
            else if (picked->handler == NULL || (picked->priority == ISN_MSG_PRI_QUERY_ARGS && obj->msgnum != obj->isn_msg_received_msgnum)) {
                send_packet(obj, obj->msgnum, NULL, 0);
                picked->priority = picked->handler ? __ISN_MSG_PRI_QUERY_WAIT : ISN_MSG_PRI_CLEAR;
                if (picked->priority == __ISN_MSG_PRI_QUERY_WAIT) obj->resend_timer = 0;
            }
            else {
                obj->handler_priority = picked->priority;
                picked->priority = ISN_MSG_PRI_CLEAR;
                if (picked->handler) {
                    obj->handler_msgnum = obj->msgnum;
                    if (obj->msgnum == obj->isn_msg_received_msgnum) {
                        data = picked->handler(obj->handler_input = (const void*)obj->isn_msg_received_data);
                        obj->isn_msg_received_msgnum = 0xFF;
                        obj->isn_msg_received_data   = NULL;        // free receive buffer
                        obj->handler_input           = NULL;
                        isn_reactor_mutex_unlock(obj->busy_mutex);  // release pending events
                    }
                    else {
                        data = (uint8_t *)picked->handler(NULL);
                    }
                    obj->handler_msgnum = -1;
                    if (data == NULL) {
                        return 1;
                    }
                    // Do not reply back if request for data was done from our side, to avoid ping-ponging
                    // Handle also the case of just-arriving QUERY_ARGS message whch is not yet in _WAIT state.
                    if (obj->handler_priority != __ISN_MSG_PRI_QUERY_WAIT && obj->handler_priority != ISN_MSG_PRI_QUERY_ARGS) {
                        send_packet(obj, obj->msgnum, data, picked->size);
                    }
                }
            }
        }
        return 1;   // there might be one or more messages to send
    }
    return 0;
}

int isn_msg_isinput_valid(isn_message_t *obj, const void *arg) {
    return (arg == obj->handler_input) && arg;
}

/** Schedule pending messages and if sched returns more work, re-trigger the event */
static void *emit_event(void *arg) {
    return isn_msg_sched((isn_message_t *)arg) > 0 ? &emit_event : NULL;
}

/**
 * Set pending flag, and trigger the sched event if reactor is set
 */
static void emit(isn_message_t *obj) {
    if (!obj->pending) {
        obj->pending = 1;
        if (obj->queue) obj->queue(emit_event, obj, ISN_CLOCK_NOW, obj->holdon_mutex);
    }
}

void isn_msg_post(isn_message_t *obj, uint8_t message_id, uint8_t priority) {
    // Ignore out-of-table requests; \todo we need to add query for LAST
    if (message_id >= obj->isn_msg_table_size) return;

    uint8_t s = CyEnterCriticalSection();
    if (priority == ISN_MSG_PRI_CLEAR) {
        obj->isn_msg_table[message_id].priority = priority;
    }
    // Ignore zero-arg messages as these can appear as queries to the IDM, but allow desc
    else if (obj->isn_msg_table[message_id].size || priority >= ISN_MSG_PRI_DESCRIPTIONLOW) {
        if (obj->isn_msg_table[message_id].priority < priority) obj->isn_msg_table[message_id].priority = priority;
        emit(obj);
    }
    CyExitCriticalSection(s);

    if (obj->dup && priority <= ISN_MSG_PRI_HIGHEST) isn_msg_post(obj->dup, message_id, priority);
}

void isn_msg_send(isn_message_t *obj, uint8_t message_id, uint8_t priority) {
    if (obj->handler_msgnum != message_id) {         // Do not mark and trigger a message from which we're called
        isn_msg_post(obj, message_id, priority);
    }
}

uint8_t isn_msg_sendqby(isn_message_t *obj, isn_events_handler_t hnd, uint8_t priority, uint8_t msgnum) {
	for (; msgnum < obj->isn_msg_table_size; msgnum++) {
        if (obj->isn_msg_table[msgnum].handler == hnd) {
            isn_msg_send(obj, msgnum, priority);
            return msgnum;
        }
    }
    return 0xff;
}

uint8_t isn_msg_resend_queries(isn_message_t *obj, uint32_t timeout) {
    uint8_t count = 0;
    if (obj->resend_timer < INT32_MAX) obj->resend_timer++;
    if (obj->resend_timer > timeout) {
        /* Convert lock into a new pending message */
        if (obj->lock) {
            obj->isn_msg_table[obj->lock].priority = ISN_MGG_PRI_UPDATE_ARGS;
            obj->lock = 0;
        }
        /* Check all messages with QUERY_WAIT as well as UPDATE_ARGS to schedule retries */
        for (uint8_t msgnum = 0; msgnum < obj->isn_msg_table_size; msgnum++) {
            if (obj->isn_msg_table[msgnum].priority == __ISN_MSG_PRI_QUERY_WAIT) {
                obj->isn_msg_table[msgnum].priority = ISN_MSG_PRI_QUERY_ARGS;
                count++;
                obj->drv.stats.tx_retries++;
            }
            /* Theoretically this is not needed, however it is an additional protection to
               account this type of pending messages and to increase count and trigger
               pending state, which could be missed if lock was already 0 */
            if (obj->isn_msg_table[msgnum].priority == ISN_MGG_PRI_UPDATE_ARGS) {
                count++;
                obj->drv.stats.tx_retries++;
            }
        }
    }
    if (count) {
        obj->resend_timer = 0;
        emit(obj);
    }
    return count;
}

int isn_msg_discardpending(isn_message_t *obj) {
    uint8_t count = 0;
    for (uint8_t msgnum = 0; msgnum < obj->isn_msg_table_size; msgnum++) {
        if (obj->isn_msg_table[msgnum].priority > ISN_MSG_PRI_CLEAR) {
            obj->isn_msg_table[msgnum].priority = ISN_MSG_PRI_CLEAR;
            count++;
        }
    }
    obj->lock = 0;
    return count;
}

/**
 * Receive a message from low-level driver
 *
 * This driver implements single buffering, so we may process one input
 * data at a time until sendnext() sends reply. However we may handle
 * multiple requests for data since they don't provide any input data.
 */
static size_t isn_message_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    isn_message_t *obj = (isn_message_t *)drv;
    const uint8_t *buf = src;

    if (!src || size < 2 || *buf != ISN_PROTO_MSG) {
        obj->drv.stats.rx_dropped++;
        return size;
    }

    uint8_t msgnum = buf[1] & 0x7F;
    uint8_t data_size = size - 2;

#if CONFIG_ISN_MSG_FAST_LOADING > 0
    if (msgnum == ISN_MSG_NUM_LAST) {    // speed up loading and mark all mesages to be send out
        for (int i=ISN_MSG_NUM_ID+1; i<(obj->isn_msg_table_size-1); i++) {
            isn_msg_post(obj, i, (buf[1] & 0x80) ? ISN_MSG_PRI_DESCRIPTIONLOW : ISN_MSG_PRI_LOW);
        }
    }
#endif

    if (msgnum >= obj->isn_msg_table_size) { // IDM asks for the last possible, indicating it doesn't know the device, so discard input buf
        msgnum = obj->isn_msg_table_size - 1;
        data_size = 0;
    }
    if (data_size > 0) {
        // we cannot handle multiple requests currently so we retry next time
        // if data size does not match, drop complete message
        if (obj->isn_msg_received_data != NULL) return 0;
        if (data_size != obj->isn_msg_table[msgnum].size) {
            obj->drv.stats.rx_dropped++;
            return size;
        }
    }
    /* Discard data if another UPDATE ARGS for the same message is already in progress,
     * which eliminates inter-mediate receieve callbacks
     */
    if (obj->isn_msg_table[msgnum].priority != ISN_MGG_PRI_UPDATE_ARGS ) {
        if (data_size > 0) {
            assert(data_size <= RECV_MESSAGE_SIZE);
            isn_memcpy(obj->message_buffer, buf+2, data_size);   // copy recv data into a receive buffer to be handled by sched
            obj->isn_msg_received_data = obj->message_buffer;
            obj->isn_msg_received_msgnum = msgnum;
            isn_reactor_mutex_lock(obj->busy_mutex);        // buffer full we cannot accept new requests
        }
        isn_msg_post(obj, msgnum, (uint8_t) ((buf[1] & 0x80) ? ISN_MSG_PRI_DESCRIPTION : ISN_MSG_PRI_HIGHEST));
    }
    else if (msgnum == obj->lock) {
        obj->lock = 0;
        emit(obj);  // message is pending, and releasing the lock requires retriggering of sched
    }
    obj->msgnum = msgnum;   // speed-up response time to all incoming request and to release incoming buffer
    obj->drv.stats.rx_packets++;
    obj->drv.stats.rx_counter += data_size;
    return size;
}


int isn_msg_sched(isn_message_t *obj) {
    if (obj->pending) {
        if (obj->parent_driver->getsendbuf(obj->parent_driver, NULL, 1, (isn_layer_t *)obj) > 0) {    // Test if we have at least 1 byte space to send?
            obj->pending = isn_msg_sendnext(obj);
        }
    }
    return obj->pending;
}

/**
 * - clears all false pending messages without any payload that could confuse receiver,
 *   as zero payload means query
 */
static void sanity_check(isn_message_t *obj) {
	for (uint8_t i = 0; i < obj->isn_msg_table_size; i++) {
		if (obj->isn_msg_table[i].priority > ISN_MSG_PRI_CLEAR && obj->isn_msg_table[i].size == 0) {
            obj->isn_msg_table[i].priority = ISN_MSG_PRI_CLEAR;
        }
    }
}

void isn_msg_radiate(isn_message_t *obj, isn_reactor_queue_t priority_queue, isn_reactor_mutex_t busy_mutex, isn_reactor_mutex_t holdon_mutex) {
    obj->queue = priority_queue;
    obj->pending = 0;   // set to zero to re-trigger the event
    obj->busy_mutex = busy_mutex;
    obj->holdon_mutex = holdon_mutex;
    emit(obj);
}

void isn_msg_init(isn_message_t *obj, isn_msg_table_t* messages, uint8_t size, isn_layer_t* parent) {
    ASSERT(obj);
    ASSERT(messages);
    ASSERT(parent);
    memset(&obj->drv, 0, sizeof(obj->drv));
    obj->drv.recv = isn_message_recv;
    obj->parent_driver = parent;
    obj->isn_msg_table = messages;
    obj->isn_msg_table_size = size;
    obj->isn_msg_received_msgnum = 0xFF;
    obj->isn_msg_received_data = NULL;
    obj->handler_input = NULL;
    obj->handler_msgnum = -1;
    obj->handler_priority = 0;
    obj->pending = 1;
    obj->active = 0;
    obj->lock = 0;
    obj->msgnum = 0;
    obj->resend_timer = 0;
    obj->queue = NULL;  // By default reactor is not enabled and priority queue is to be set by user
    obj->dup = NULL;
    isn_msg_self = obj;
    sanity_check(obj);
}

isn_message_t* isn_msg_create() {
    isn_message_t* obj = malloc(sizeof(isn_message_t));
    return obj;
}

void isn_msg_drop(isn_message_t *obj) {
    free(obj);
}

void isn_msg_setlogging(isn_logger_level_t level) {
    isn_logger_level = level;
}


/** \} \endcond */
