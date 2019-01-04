/** \file
 */

#include <string.h>
#include "assert.h"
#include "isn_msg.h"

#define RECV_MESSAGE_SIZE           64

#define ISN_MSG_MESSAGE_COUNT       128
#define ISN_MSG_PRIORITY_COUNT      32
#define ISN_MSG_PRIORITIES_BLOCKS   ((ISN_MSG_PRIORITY_COUNT + sizeof(uint32_t) * 8 - 1) / (sizeof(uint32_t) * 8))
#define ISN_MSG_MESSAGE_BLOCKS      ((ISN_MSG_MESSAGE_COUNT + sizeof(uint32_t) * 8 - 1) / (sizeof(uint32_t) * 8))

typedef unsigned int uint_t;

static isn_layer_t* parent_driver;

static isn_msg_table_t* isn_msg_table;              ///< Ref to the message table
static uint8_t isn_msg_table_size = 0;              ///< It's size
static uint8_t message_buffer[RECV_MESSAGE_SIZE];   ///< Receive buffer
static uint8_t isn_msg_received_msgnum = 0xFF;      ///< Receive buffer's message number to which isn_msg_received_data belongs
static volatile void* isn_msg_received_data = NULL; ///< Receive buffer's pointer
static const void *handler_input = NULL;            ///< Copy of handlers input data to be used with isn_msg_isinput_valid() only
static int32_t handler_msgnum = -1;                 ///< Message number of a handler in a call
uint8_t handler_priority = 0;
static uint8_t pending = 0;


static void send_packet(uint8_t msgflags, const void* data, isn_msg_size_t size) {
    uint8_t *buf = NULL;
    size += 2;
    if (parent_driver->getsendbuf(&buf, size) == size) {
        *buf     = ISN_PROTO_MSG;
        *(buf+1) = msgflags;
        memcpy(buf+2, data, size);
        parent_driver->send(buf, size);
    }
    else {
        parent_driver->free(buf);   // we're ok to free NULL to simplify code
    }
}

/** Send next message by RR scheme */
int isn_msg_sendnext(void) {
    static uint8_t msgnum = 0;
    isn_msg_table_t* picked = NULL;
    uint8_t* data = NULL;

	for (uint8_t i = 0; i < isn_msg_table_size; msgnum++, i++) {
		if (msgnum >= isn_msg_table_size) msgnum = 0;
		if (isn_msg_table[msgnum].priority > 0) {
			picked = &isn_msg_table[msgnum];
			break;
		}
	}

    if (picked) {
        if (picked->priority >= ISN_MSG_PRI_DESCRIPTIONLOW) {            
            send_packet((uint8_t)0x80 | msgnum, picked->desc, strlen(picked->desc));
            picked->priority = (msgnum == isn_msg_received_msgnum) ? ISN_MSG_PRI_HIGHEST : ISN_MSG_PRI_LOW;
        } 
        else {
            handler_priority = picked->priority;
            picked->priority = 0;
            if (picked->handler) {
                handler_msgnum = msgnum;
                if (msgnum == isn_msg_received_msgnum) {
                    data = picked->handler(handler_input = (const void*) isn_msg_received_data);
                    isn_msg_received_msgnum = 0xFF;
                    isn_msg_received_data   = NULL;     // free receive buffer                
                    handler_input           = NULL;
                }
                else {
                    data = (uint8_t *)picked->handler(NULL);
                }
                handler_msgnum = -1;
                if (data == NULL) return 0;
            }
            send_packet(msgnum, data, picked->size);
        }
        return 1;
    }
    return 0;
}


int isn_msg_isinput_valid(const void *arg) {
    return (arg == handler_input) && arg;
}


static void isn_msg_post(uint8_t message_id, uint8_t priority) {
    if (isn_msg_table[message_id].priority < priority) {
        isn_msg_table[message_id].priority = priority;  // we only want to increase priority and never reduce
    }
    pending = 1;
}


void isn_msg_send(uint8_t message_id, uint8_t priority) {
    if (handler_msgnum != message_id) {         // Do not mark and trigger a message from which we're called
        isn_msg_post(message_id, priority);
    }
}


uint8_t isn_msg_sendqby(isn_events_handler_t hnd, uint8_t priority, uint8_t msgnum) {
	for (msgnum = 0; msgnum < isn_msg_table_size; msgnum++) {
        if (isn_msg_table[msgnum].handler == hnd) {
            isn_msg_send(msgnum, priority);
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
const uint8_t * isn_message_recv(const uint8_t *buf, size_t size, isn_layer_t *caller) {
    uint8_t data_size = size - 2;
    uint8_t msgnum = buf[1] & 0x7F;

    if (msgnum == ISN_MSG_NUM_LAST) {    // speed up loading and mark all mesages to be send out
        for (int i=ISN_MSG_NUM_ID+1; i<(isn_msg_table_size-1); i++) {
            isn_msg_post(i, buf[1] & 0x80 ? ISN_MSG_PRI_DESCRIPTIONLOW : ISN_MSG_PRI_LOW);
        }
    }
    if (msgnum >= isn_msg_table_size) { // IDM asks for the last possible, indicating it doesn't know the device, so discard input buf
        msgnum = isn_msg_table_size - 1;
        data_size = 0;
    }
    if (data_size > 0 && (isn_msg_received_data != NULL || data_size != isn_msg_table[msgnum].size)) {
        return buf;                     // we cannot handle multiple receive buffer requests atm, nor wrong input sizes
    }
    if (data_size > 0) {
        assert(data_size <= RECV_MESSAGE_SIZE);
        memcpy(message_buffer, buf+2, data_size);   // copy recv data into a receive buffer to be handled by sched
        isn_msg_received_data = message_buffer;
        isn_msg_received_msgnum = msgnum;
    }
    isn_msg_post(msgnum, (uint8_t) (buf[1] & 0x80 ? ISN_MSG_PRI_DESCRIPTION : ISN_MSG_PRI_HIGHEST));
    return buf;
}


void isn_msg_sched() {
    if (pending) {
        if (parent_driver->getsendbuf(NULL, 1) > 0) {    // Test if we have at least 1 byte space to send?
            pending = isn_msg_sendnext();
        }
    }
}


void isn_msg_init(isn_msg_table_t* messages, uint8_t size, isn_layer_t* parent) {
    isn_msg_table      = messages;
    isn_msg_table_size = size;
    parent_driver      = parent;
    pending            = 1;
}


isn_layer_t isn_message = {
    NULL,
    NULL, 
    isn_message_recv,
    NULL
};
