/** \file
 *  \brief ISN Message Layer
 *  \author Uros Platise <uros@isotel.org>
 *  \see https://www.isotel.org/isn/message.html
 */
/**
 * \ingroup GR_ISN
 * \defgroup GR_ISN_Message Message Layer Driver
 *
 * # Scope
 *
 * Implements Device side of the [ISN Message Layer Protocol](https://www.isotel.org/isn/message.html)
 * with direct mapping of C structures to messages and easy to use callback API.
 * A message layer creates a virtual device at the external entity that may hold up to 128
 * messages, each long of about 128 bytes (64 bytes for description and 64 bytes for variable
 * arguments). The system may implement numerous Message Layers thus creating more than just
 * on device at the external entity with the help of other ISN protocol layers.
 *
 * Message Layers descriptors can be very powerful and provide:
 *
 * - text elements, sections, basic, advanced, development and hidden sections
 * - data structures and parameter - value like expressions
 * - tabular (vertical and horizontal table) representation
 * - arguments of little and big-endian formats of standard and custom bit-lengths
 * - math expressions, with cross-referencing to other variables from other messages
 * - accuracy support with correlated cross-refs propagation
 * - enumerations with filters, and push-buttons
 * - severity information to propagate information as info, warning, or error
 *
 * # Concept
 *
 * Each message is represented by a C structure, which needs to be packed (not padded).
 * For example: for a three colour LED a structure would look like:
 *
 * ~~~
 * typedef struct {
 *   uint8_t red;
 *   uint8_t green;
 *   uint8_t blue;
 * } __attribute__((packed)) led_t;
 *
 * led_t led;
 * ~~~
 *
 * To interact with the Message layer it needs a callback handler.
 *
 * ~~~
 * void *led_cb(const void *data) {
 *   if (data) {
 *       led = *((const led_t *)data);
 *       LED_PWM8_R_Write(led.red);
 *       LED_PWM8_G_Write(led.green);
 *       LED_PWM8_B_Write(led.blue);
 *   }
 *   return &led;
 * }
 * ~~~
 *
 * It is called in the following three cases:
 *
 * 1. when external entity wants to modify its value, the incoming data is provided by the
 *    `data` pointer holding exactly the same structure; A callback should do incoming range
 *    check to ensure data integrity on critical parameters.
 * 2. when external entity wants to read the latest state, in which case the `data` is NULL,
 * 3. and when the device itself wants to send out an update using the isn_msg_send() method,
 *    in which case the `data` is also NULL, however a message can be scheduled at different
 *    priorities compared to the 2nd case.
 *
 * C type structures and callbacks are bundled together with the message
 * descriptors in the so-called message table. A minimal table looks like this:
 *
 * ~~~
 * static isn_msg_table_t isn_msg_table[] = {
 *   { 0, 0,             NULL,   "%T0{MyCompany FlashLight} {#sno}={12345678}" },
 *   { 0, sizeof(led_t), led_cb, "LED {:red}={%hu}{:green}={%hu}{:blue}={%hu}" },
 *   ISN_MSG_DESC_END(0)
 * };
 * ~~~
 *
 * where:
 *
 * 1. The first message is a mandatory unique identifier of the device and typically
 *    also holds a C structure with a callback handler to read out silicon ID, etc.
 *    Name should be composed of a vendor - product free string.
 * 2. The 2nd message is the LED accompanied with the three printf() like descriptions
 * 3. The 3rd message is the mandatory last terminator message and may in addition
 *    provide device checksum.
 *
 * The table is then passed to the isn_msg_init(). The parent protocol (`isn_parent_protocol`)
 * will act as stimulus from the interface side, and the device itself posts message by
 * isn_msg_send() and isn_msg_sendqby() methods. The main loop handles these requests
 * in the main-loop by calling isn_msg_sched() or these are automatically handled by
 * reactor, use isn_msg_radiate() to enable this feature.
 * ~~~
 * isn_message_t isn_message;
 *
 * isn_msg_init(&isn_message, isn_msg_table, ARRAY_SIZE(isn_msg_table), &isn_parent_protocol);
 *
 * while(1) {
 *     if ( !isn_msg_sched(&isn_message) ) {
 *         // All done so we may fall asleep. Low-level ISRs will wake us up
 *         asm volatile("wfi");
 *     }
 * }
 * ~~~
 *
 * and the `isn_parent_protocol` is the parent protocol layer whose child this particular message is.
 * A device can implement numerous virtual devices (message layers).
 *
 * # Priorities
 *
 * Message layer uses priorities to determine which message to send out first:
 *
 * 1. Internally request for message descriptors is given the highest priority.
 *    It ensures that external entity can retrieve message structure even when
 *    it is flooded with the data. Defined by the `ISN_MSG_PRI_DESCRIPTION`
 * 2. The second highest is required for arguments by external entity, given by
 *    `ISN_MSG_PRI_HIGHEST`
 * 3. When user posts a message to be sent out with the isn_msg_send() method
 *    it should specify the priority from `ISN_MSG_PRI_LOW` to `ISN_MSG_PRI_HIGHEST`
 *    preferably by using macros.
 *
 * # Requesting for Data or Updating the Data
 *
 * Message layer allows to send request to other device for arguments using the
 * isn_msg_send() or isn_msg_sendqby() and providing ISN_MSG_PRI_QUERY_ARGS
 * for the priority field. After a request to receive the arguments is sent, message is
 * locked into the ISN_MSG_PRI_QUERY_WAIT state, in which it
 * will not send out any data until a valid reply is received.
 *
 * Example of a communication flow between two devices:
 *\msc
 *  width = "1000";
 *  R [label="Requester"], A [label="Message Layer"], B [label="Message Layer"], T [label="Target"];
 *
 *  R => A [label="isn_msg_sendqby(.., my_message_cb, ISN_MSG_PRI_QUERY_ARGS)"];
 *  A -x B [label="ISN_MSG_PRI_QUERY_ARGS"];
 *  A => A [label="ISN_MSG_PRI_QUERY_WAIT"];
 *  |||;
 *  --- [label="Waiting for response, which never arrives"];
 *  |||;
 *  A => A [label="isn_msg_resend_queries"];
 *  A -> B [label="ISN_MSG_PRI_QUERY_ARGS"];
 *  A => A [label="ISN_MSG_PRI_QUERY_WAIT"];
 *  B => T [label="my_message_cb()"];
 *  B << T [label="return &data"];
 *  B -> A [label="ISN_MSG_PRI_HIGHEST"];
 *  A => A [label="ISN_MSG_PRI_CLEAR"];
 *  R <<= A [label="my_message_cb(data)"];
 *\endmsc
 *
 * Similarly one devices needs to update arguments (send data) to
 * other device, which is done by setting the priority ISN_MGG_PRI_UPDATE_ARGS.
 * Message layer itself employs single input buffer. At higher reception rate
 * buffer is to be provided by the receiving layer. However, when
 * ISN_MGG_PRI_UPDATE_ARGS is used to send a message, then further transmissions
 * are blocked to ensure buffer overflow does not happen at the receiving device.
 * Updating arguments handle another special case:
 *
 * - while transaction is in progress, requester may send another update,
 *   and thus ignoring previous transaction, looks like this:
 *
 *\msc
 *  width = "1000";
 *  R [label="Requester"], A [label="Message Layer"], B [label="Message Layer"], T [label="Target"];
 *
 *  R => A [label="isn_msg_sendqby(.., my_message_cb, ISN_MSG_PRI_UPDATE_ARGS)"];
 *  R <<= A [label="my_message_cb()"];
 *  R >> A [label="return &data"];
 *  A -> B [label="ISN_MSG_PRI_UPDATE_ARGS"];
 *  A => A [label="sending locked"];
 *  B => T [label="my_message_cb(data)"];
 *  B << T [label="return &data"];
 *  |||;
 *  --- [label="Updated Request"];
 *  |||;
 *  R => A [label="isn_msg_sendqby(.., my_message_cb, ISN_MSG_PRI_UPDATE_ARGS)"];
 *  B -> A [label="ISN_MSG_PRI_HIGHEST"];
 *  A => A [label="ignored"];
 *  R <<= A [label="my_message_cb()"];
 *  R >> A [label="return &data"];
 *  A -> B [label="ISN_MSG_PRI_UPDATE_ARGS"];
 *  B => T [label="my_message_cb(data)"];
 *  B << T [label="return &data"];
 *  B -> A [label="ISN_MSG_PRI_HIGHEST"];
 *  A => A [label="sending unlocked"];
 *  R <<= A [label="my_message_cb(data)"];
 *\endmsc
 *
 * Unlocking a message from this state occurs when:
 *
 * - by periodically calling the isn_msg_resend_queries() which resends pending
 *   transactions, both ISN_MSG_PRI_QUERY_ARGS and the ISN_MGG_PRI_UPDATE_ARGS,
 * - or by using the priority ISN_MSG_PRI_CLEAR to clear specific requests, or
 * - if other party requests to receive a description, or it is send by this
 *   device providing ISN_MSG_PRI_DESCRIPTION to priority, which clearly
 *   means that the other device has no clue about us, so it cannot provide
 *   us with the arguments
 *
 * If other device requests arguments while a message is in the
 * ISN_MSG_PRI_QUERY_WAIT state, such requests are ignored. If none
 * of the devices wants to transmit a data a dead-lock could occur.
 * To exit from such scenario one of the party must tell the other
 * that it is unaware of its content, which is achieved with the
 * request for description. Not knowing descriptors means having
 * insufficient information. Besides receiving a request for descriptors
 * a normal request for args follows (without data). Device therefore
 * uses its internal defaults.
 *
 * This mode can be used for P2P communication where one is a host,
 * or requesting party, and the other the device. Peer requesting the
 * arguments from the other party waits for arguments in the
 * ISN_MSG_PRI_QUERY_WAIT. Upon reception of arguments a callback
 * handler is called and the state-machine will not send confirmation
 * packet back to the host; to prevent end-less ping-pong situation.
 *
 * Each party is responsible for receiving what they want.
 * Device may call with a slow-timer once per second, or per 2 seconds,
 * the isn_msg_resend_queries() which will re-schedule all of the
 * unanswered requests. Function returns number of pending requestions,
 * so a device may just monitor this return value to identify if
 * other peer is responding correctly. When all requests are handled
 * function returns 0 (pending requests).
 */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) Copyright 2019 - 2021, Isotel, http://isotel.org
 */


#ifndef __ISN_MSG_H__
#define __ISN_MSG_H__

#include "isn_def.h"
#include "isn_reactor.h"
#include "isn_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------*/
/* CONFIGURATION                                                      */
/*--------------------------------------------------------------------*/

/** Enable fast loading of messages
 *
 * When the msg layer receives a query, desc or arg, for the ISN_MSG_NUM_LAST
 * message, it will mark all messages, for desc or arg to be sent out.
 * Note that arg messages that are zero-sized are not sent.
 *
 * Drastical speed out allows a single query for all desc, and a single
 * query for all args.
 */
#ifndef CONFIG_ISN_MSG_FAST_LOADING
# define CONFIG_ISN_MSG_FAST_LOADING 0
#endif

/** A single query for args can be sent out and further requests are blockd until reply is received.
 *  If zero, then multiple queries are sent out at as fast as possible.
 */
#ifndef CONFIG_ISN_MSG_SINGLE_QUERY
# define CONFIG_ISN_MSG_SINGLE_QUERY 0
#endif

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

#define UINT8_NaN                   0
#define INT8_NaN                    INT8_MIN

#define UINT16_NaN                  0
#define INT16_NaN                   INT16_MIN

#define UINT32_NaN                  0
#define INT32_NaN                   INT32_MIN

#define ISN_MSG_NUM_UNKOWN          0       ///< Used with the isn_msg_sendqby(handler, priority, MSG_NUM_UNKNOWN) for the first time
#define ISN_MSG_NUM_ID              0
#define ISN_MSG_NUM_LAST            127

#define ISN_MSG_PRI_DESCRIPTION     31      ///< Standard high priority description
#define ISN_MSG_PRI_DESCRIPTIONLOW  30      ///< Lower priority description, used in fast loading
//#define ISN_MSG_PRI_QUERY_DESC    28      ///< Request for description, NOT SUPPORTED YET
#define ISN_MSG_PRI_QUERY_ARGS      27      ///< Request for arguments
#define ISN_MSG_PRI_QUERY_WAIT      26      ///< Wait for reply, internal intermediate state do NOT USE
#define ISN_MGG_PRI_UPDATE_ARGS     25      ///< Send an update and block further transmission
#define ISN_MSG_PRI_HIGHEST         0x0f    ///< When other peer requests args, these are send with this priority
#define ISN_MSG_PRI_HIGH            0x08
#define ISN_MSG_PRI_NORMAL          0x04
#define ISN_MSG_PRI_LOW             0x01
#define ISN_MSG_PRI_CLEAR           0x00

#define ISN_MSG_DESC_END(pri)       { pri, 0, NULL, "%!" }

typedef uint8_t isn_msg_size_t;

typedef struct {
    volatile uint8_t     priority;  ///< 0 when done, priority, higher value higher priority
    isn_msg_size_t       size;      ///< size of data
    isn_events_handler_t handler;   ///< callback handler, or NULL if a message contains no arguments
    const char*          desc;      ///< pointer to message descriptor
}
isn_msg_table_t;

extern uint8_t handler_priority;

#define RECV_MESSAGE_SIZE           64

/** Internal struct, note the alignment of the message_buffer, which should be aligned to (4)
 *  More info on align: https://stackoverflow.com/questions/4306186/structure-padding-and-packing
 */
typedef struct isn_message_s {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* parent_driver;
    isn_msg_table_t* isn_msg_table;             ///< Ref to the message table
    uint8_t message_buffer[RECV_MESSAGE_SIZE];  ///< Receive buffer
    uint8_t isn_msg_table_size;                 ///< It's size
    uint8_t isn_msg_received_msgnum;            ///< Receive buffer's message number to which isn_msg_received_data belongs
    void* isn_msg_received_data;                ///< Receive buffer's pointer
    const void *handler_input;                  ///< Copy of handlers input data to be used with isn_msg_isinput_valid() only
    int32_t handler_msgnum;                     ///< Message number of a handler in a call
    uint8_t handler_priority;
    uint8_t pending;
    uint8_t active;                             ///< Number of active messages
    uint8_t msgnum;                             ///< Last msgnum sent
    uint8_t lock;                               ///< Lock, to prevent sending further messages, when waiting for ack (reply)
    uint32_t resend_timer;

    struct isn_message_s *dup;                  ///< Duplicate updates to another message layer (i.e. for tracing or cross-updating)

    isn_reactor_queue_t queue;                  ///< Reactor queue
    isn_reactor_mutex_t busy_mutex;             ///< Controlled by msg layer when busy
    isn_reactor_mutex_t holdon_mutex;           ///< Controlled by 3rd layer to delay execution
}
isn_message_t;

extern isn_message_t *isn_msg_self;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

isn_message_t* isn_msg_create();

void isn_msg_drop(isn_message_t *obj);

/** Initialize Message Layer
 *
 * \param obj
 * \param messages a pointer to application table of messages
 * \param size of the table, which is equalt to number of messages in a table
 * \param parent object
 */
void isn_msg_init(isn_message_t *obj, isn_msg_table_t* messages, uint8_t size, isn_layer_t* parent);

/** Enable reactor
 *
 * \param obj
 * \param priority_queue sets the queue
 * \param busy_mutex controlled by the message layer to retain pending events until ready
 * \param holdon_mutex controlled by parent layers to get ready before message layer can push new messages
 */
void isn_msg_radiate(isn_message_t *obj, isn_reactor_queue_t priority_queue, isn_reactor_mutex_t busy_mutex, isn_reactor_mutex_t holdon_mutex);

/** Duplicate message updates
 * 
 * \param obj
 * \param dup message layer to which requests are duplicated for all priority <= ISN_MSG_PRI_HIGHEST
 */
static inline void isn_msg_dup(isn_message_t *obj, isn_message_t *dup) {obj->dup = dup;};

/** Schedule received callbacks and send those marked by isn_msg_send() or isn_msg_sendby()
 *
 * \param obj
 * \returns 0 when no more messages are pending for transmission
 */
int isn_msg_sched(isn_message_t *obj);

/**
 * Post message by id, interrupt/thread safe
 *
 * Similar to isn_msg_send() but this one requires an explicit message id to avoid for looping
 * from the interrupt calls.
 *
 * \param obj
 * \param message_id
 * \param priority defines order in which messages are sent out.
 *        Setting MSB bit 0x80 will request to send out descriptors instead of a data
 *
 * We only want to increase priority and never reduce.
 *
 * \note So that's why request for DESC "clears" request for QUERY which obviously means
 * that the other device have no information about this device, so it cannot serve
 * it with args yet.
 */
void isn_msg_post(isn_message_t *obj, uint8_t message_id, uint8_t priority);

/** Send message
 *
 * \see isn_msg_post for interrupt/thread safe call
 *
 * \param obj
 * \param message_id
 * \param priority defines order in which messages are sent out.
 *        Setting MSB bit 0x80 will request to send out descriptors instead of a data
 */
void isn_msg_send(isn_message_t *obj, uint8_t message_id, uint8_t priority);

/** Send message quickly by callback handler given msgnum, start of the search
 *
 * Typical usage:
 * \code
 *    uint8_t msg1_idx = 0;
 *    msg1_idx = isn_msg_sendqby(obj, msg1_cb, ISN_MSG_PRI_NORMAL, msg1_idx);
 * \endcode
 *
 * \param obj
 * \param hnd
 * \param priority defines order in which messages are sent out.
 *        Setting MSB bit 0x80 will request to send out descriptors instead of a data
 * \param msgnum index to message_id previously provided by this same function to speed up the search.
 * \returns message_id on success otherwise 0xFF
 */
uint8_t isn_msg_sendqby(isn_message_t *obj, isn_events_handler_t hnd, uint8_t priority, uint8_t msgnum);

/** Send message by callback handler
 *
 * \param obj
 * \param hnd
 * \param priority defines order in which messages are sent out.
 * \return message_id on success otherwise 0xFF
 */
static inline uint8_t isn_msg_sendby(isn_message_t *obj, isn_events_handler_t hnd, uint8_t priority) {return isn_msg_sendqby(obj,hnd,priority,0);}

/** Resend all pending queries
 *
 * This function may be called with a (retry) timer, every since (1 - 3 seconds) to
 * ensure other party responds to the messages. Function returns number of still pending
 * messages that have been rescheduled for transmission. If within certain time this
 * value is not 0 means other party is not responding, indicating some issues on the other
 * side. The parameter timeout if 0, means that rescheduling is done on each call
 * for events marked as ISN_MGG_PRI_UPDATE_ARGS and ISN_MSG_PRI_QUERY_WAIT. If timeout
 * is 1, or higher, then re-scheduling starts after 1, or 2, call to this function.
 *
 * \param obj
 * \param timeout as a number of calls to this routine after which resend actually takes progress
 * \returns Message count marked for re-transmission
 */
uint8_t isn_msg_resend_queries(isn_message_t *obj, uint32_t timeout);

/**
 * Discard just all pending messages, includes pending queries, updates, etc.
 * \returns number of discarded pending messages
 */
int isn_msg_discardpending(isn_message_t *obj);

/**
 * Callback may call this function to confirm that input relates
 * to a callback argument from message layer.
 *
 * Useful if same callback event is called from multiple sources.
 *
 * \param obj
 * \param arg pointer to data provided by the callback
 * \returns Non-zero if valid and argument is non-zero, otherwise 0
 */
int isn_msg_isinput_valid(isn_message_t *obj, const void *arg);

/**
 * Set logger (debugging) level
 */
void isn_msg_setlogging(isn_logger_level_t level);

/**
 * Returns number of active messages
 *
 * \param obj
 * \return 0 if none, otherwise >0
 */
static inline int isn_msg_noactive(isn_message_t *obj) { return obj->active; }

/**
 * Is message being handled, either transaction, query, simple message sending, etc.
 *
 * \param obj
 * \param msgnum index to message_id previously provided by this same function to speed up the search.
 */
static inline int isn_msg_isdone(isn_message_t *obj, uint8_t msgnum) { return obj->isn_msg_table[msgnum].priority == ISN_MSG_PRI_CLEAR;}

/**
 * To be used within the callback, it may ask with which priority
 * was it called to be able to distinguish also from external
 * (HIGHEST) queries and internal ones
 *
 * \param obj
 * \returns Non-zero if request is query
 */
static inline int isn_msg_isquery(isn_message_t *obj) {return obj->handler_priority == ISN_MSG_PRI_HIGHEST;}

/**
 * To be used within the callback, it may ask if data is a
 * reply to previously sent out query.
 *
 * \param obj
 * \returns non-zero if message is a response to a ISN_MSG_PRI_QUERY_ARGS or just
 *          in-time received message (clears ISN_MSG_PRI_QUERY_ARGS)
 */
static inline int isn_msg_isreply(isn_message_t *obj) {
    return obj->handler_priority == ISN_MSG_PRI_QUERY_WAIT || obj->handler_priority == ISN_MSG_PRI_QUERY_ARGS;
}

#ifdef __cplusplus
}
#endif

#endif
