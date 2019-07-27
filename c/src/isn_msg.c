/** \file
 *  \author Uros Platise <uros@isotel.eu>
 *  \see isn_msg.h
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * (c) Copyright 2019, Isotel, http://isotel.eu
 */


#include <string.h>
#include "assert.h"
#include "isn_msg.h"

static void send_packet(isn_message_t *obj, uint8_t msgflags, const void* data, isn_msg_size_t size) {
    void *dest = NULL;
    int xsize = size + 2;
    if (obj->parent_driver->getsendbuf(obj->parent_driver, &dest, xsize) == xsize) {
        uint8_t *buf = dest;
        *buf     = ISN_PROTO_MSG;
        *(buf+1) = msgflags;
        memcpy(buf+2, data, size);
        obj->parent_driver->send(obj->parent_driver, buf, xsize);
    }
    else {
        obj->parent_driver->free(obj->parent_driver, dest);   // we're ok to free NULL to simplify code
    }
}

/** Send next message by RR scheme */
static int isn_msg_sendnext(isn_message_t *obj) {
    isn_msg_table_t* picked = NULL;
    uint8_t* data = NULL;

	for (uint8_t i = 0; i < obj->isn_msg_table_size; obj->msgnum++, i++) {
		if (obj->msgnum >= obj->isn_msg_table_size) obj->msgnum = 0;
		if (obj->isn_msg_table[obj->msgnum].priority > 0) {
			picked = &obj->isn_msg_table[obj->msgnum];
			break;
		}
	}

    if (picked) {
        if (picked->priority >= ISN_MSG_PRI_DESCRIPTIONLOW) {            
            send_packet(obj, (uint8_t)0x80 | obj->msgnum, picked->desc, strlen(picked->desc));
            picked->priority = (obj->msgnum == obj->isn_msg_received_msgnum) ? ISN_MSG_PRI_HIGHEST : ISN_MSG_PRI_LOW;
        } 
        else {
            obj->handler_priority = picked->priority;
            picked->priority = 0;
            if (picked->handler) {
                obj->handler_msgnum = obj->msgnum;
                if (obj->msgnum == obj->isn_msg_received_msgnum) {
                    data = picked->handler(obj->handler_input = (const void*)obj->isn_msg_received_data);
                    obj->isn_msg_received_msgnum = 0xFF;
                    obj->isn_msg_received_data   = NULL;     // free receive buffer
                    obj->handler_input           = NULL;
                }
                else {
                    data = (uint8_t *)picked->handler(NULL);
                }
                obj->handler_msgnum = -1;
                if (data == NULL) return 0;
            }
            send_packet(obj, obj->msgnum, data, picked->size);
        }
        return 1;
    }
    return 0;
}


int isn_msg_isinput_valid(isn_message_t *obj, const void *arg) {
    return (arg == obj->handler_input) && arg;
}


static void isn_msg_post(isn_message_t *obj, uint8_t message_id, uint8_t priority) {
    if (obj->isn_msg_table[message_id].priority < priority) {
        obj->isn_msg_table[message_id].priority = priority;  // we only want to increase priority and never reduce
    }
    obj->pending = 1;
}


void isn_msg_send(isn_message_t *obj, uint8_t message_id, uint8_t priority) {
    if (obj->handler_msgnum != message_id) {         // Do not mark and trigger a message from which we're called
        isn_msg_post(obj, message_id, priority);
    }
}


uint8_t isn_msg_sendqby(isn_message_t *obj, isn_events_handler_t hnd, uint8_t priority, uint8_t msgnum) {
	for (msgnum = 0; msgnum < obj->isn_msg_table_size; msgnum++) {
        if (obj->isn_msg_table[msgnum].handler == hnd) {
            isn_msg_send(obj, msgnum, priority);
            return msgnum;
        }
    }
    return 0xff;
}

/**
 * Receive a message from low-level driver
 * 
 * This driver implements single buffering, so we may process one input 
 * data at a time until sendnext() sends reply. However we may handle
 * multiple requests for data since they don't provide any input data.
 */
static const void * isn_message_recv(isn_layer_t *drv, const void *src, size_t size, isn_driver_t *caller) {
    isn_message_t *obj = (isn_message_t *)drv;
    const uint8_t *buf = src;
    uint8_t data_size = size - 2;
    uint8_t msgnum = buf[1] & 0x7F;

#ifndef FASTLOAD_BUG
    if (msgnum == ISN_MSG_NUM_LAST) {    // speed up loading and mark all mesages to be send out
        for (int i=ISN_MSG_NUM_ID+1; i<(obj->isn_msg_table_size-1); i++) {
            isn_msg_post(obj, i, buf[1] & 0x80 ? ISN_MSG_PRI_DESCRIPTIONLOW : ISN_MSG_PRI_LOW);
        }
    }
#endif
    if (msgnum >= obj->isn_msg_table_size) { // IDM asks for the last possible, indicating it doesn't know the device, so discard input buf
        msgnum = obj->isn_msg_table_size - 1;
        data_size = 0;
    }
    if (data_size > 0 && (obj->isn_msg_received_data != NULL || data_size != obj->isn_msg_table[msgnum].size)) {
        return buf;                     // we cannot handle multiple receive buffer requests atm, nor wrong input sizes
    }
    if (data_size > 0) {
        assert(data_size <= RECV_MESSAGE_SIZE);
        memcpy(obj->message_buffer, buf+2, data_size);   // copy recv data into a receive buffer to be handled by sched
        obj->isn_msg_received_data = obj->message_buffer;
        obj->isn_msg_received_msgnum = msgnum;
    }
    isn_msg_post(obj, msgnum, (uint8_t) (buf[1] & 0x80 ? ISN_MSG_PRI_DESCRIPTION : ISN_MSG_PRI_HIGHEST));
    return buf;
}


int isn_msg_sched(isn_message_t *obj) {
    if (obj->pending) {
        if (obj->parent_driver->getsendbuf(obj->parent_driver, NULL, 1) > 0) {    // Test if we have at least 1 byte space to send?
            obj->pending = isn_msg_sendnext(obj);
        }
    }
    return obj->pending;
}


void isn_msg_init(isn_message_t *obj, isn_msg_table_t* messages, uint8_t size, isn_layer_t* parent) {
    obj->drv.getsendbuf = NULL;
    obj->drv.send = NULL;
    obj->drv.recv = isn_message_recv;
    obj->drv.free = NULL;
    obj->parent_driver = parent;
    obj->isn_msg_table = messages;
    obj->isn_msg_table_size = size;
    obj->isn_msg_received_msgnum = 0xFF;
    obj->isn_msg_received_data = NULL;
    obj->handler_input = NULL;
    obj->handler_msgnum = -1;
    obj->handler_priority = 0;
    obj->pending = 1;
    obj->msgnum = 0;
}
