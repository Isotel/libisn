/** \file
 *  \brief Isotel Sensor Network Message Layer Implementation
 *  \author <uros@isotel.eu>
 *  \see https://www.isotel.eu/isn/message.html
 *  
 * Message Scheduler
 * 
 * 1. Each message is set a priority, lower priority events are sent out first, if two
 *    messages share the priority then lower in number is sent out first.
 * 
 * 2. Before message is sent, message handler is called, which returns pointer to data
 *    to be sent out.
 * 
 * 3. Callback is also called when query comes with input data, and pointer to the input
 *    data.
 * 
 * 
 * Process flow
 * 
 * 1. Program internally updates structures and marks messages for transmissions.
 *    Priority can be defined as lower msgnum first, or round-robin
 * 
 * 2. Remote may query for a message(s) which should rise priority to respond timely.
 *    Priority of query messages can still be less than some 'message number' i.e. 1, or 2,
 *    which are used for timely streaming. Queries should have high priorities, since 
 *    slow-response may stall transfers with the IoT.
 * 
 * 
 * Buffering
 * 
 * 1. Query receive buffer, must hold message as long it is not handled by the callback.
 *    Note that driver may transmit another message with higher priority. If interface
 *    depends on lower-level protocols, these may include additional buffering.
 * 
 * 2. Transmit buffer, that is controlled by the user's callback, he may or may not copy
 *    it's data to it. I.e. for static parameters under user control this is not necessary.
 * 
 * 
 * Typical Callback Routine
 * 
 * void* callback_handler(void *args)
 * {
 *    if (args) // size of data is already checked by msg driver
 *      process_input_data
 * 
 *    .. prepare output data like take latest measurements ..
 * 
 *    // may copy output data to predefined msg output buffer (since only one is used at a time)
 *    // may return NULL if there is no data available.
 *    p = copy(mydata)
 *    return p;
 * }
 */

#ifndef __ISN_MSG_H__
#define __ISN_MSG_H__

#include "isn.h"

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

#define UINT8_NaN                   0
#define INT8_NaN                    INT8_MIN

#define UINT16_NaN                  0
#define INT16_NaN                   INT16_MIN

#define UINT32_NaN                  0
#define INT32_NaN                   INT32_MIN

#define ISN_MSG_NUM_UNKOWN          0   ///< Used with the isn_msg_sendqby(handler, priority, MSG_NUM_UNKNOWN) for the first time
#define ISN_MSG_NUM_ID              0
#define ISN_MSG_NUM_LAST            127

#define ISN_MSG_PRI_DESCRIPTION     31
#define ISN_MSG_PRI_DESCRIPTIONLOW  30
#define ISN_MSG_PRI_HIGHEST         0x0f
#define ISN_MSG_PRI_HIGH            0x08
#define ISN_MSG_PRI_NORMAL          0x04
#define ISN_MSG_PRI_LOW             0x01

#define ISN_MSG_DESC_END            { 1, 0, NULL, "%!" }

#define ISN_MSG_TABLE_SIZE(table)   (sizeof(table) / sizeof(isn_msg_table_t))

typedef uint8_t isn_msg_size_t;

typedef struct {
    volatile uint8_t     priority;  ///< 0 when done, priority, higher value higher priority
    isn_msg_size_t       size;      ///< size of data
    isn_events_handler_t handler;   ///< callback handler, or NULL if a message contains no arguments
    const char*          desc;      ///< pointer to message descriptor
} isn_msg_table_t;

extern uint8_t handler_priority;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** ISN Layer Driver */
extern isn_layer_t isn_message;

/** Initialize Message Layer
 * 
 * \arg message a pointer to application table of messages
 * \arg size of the table, which is equalt to number of messages in a table
 */
void isn_msg_init(isn_msg_table_t* messages, uint8_t size, isn_layer_t* parent);


/** Schedule received callbacks and send those marked by isn_msg_send() or isn_msg_sendby() */
void isn_msg_sched();

/** Send message
 *
 * \param message_id
 * \param priority defines order in which messages are sent out.
 *        Setting MSB bit 0x80 will request to send out descriptors instead of a data
 */
void isn_msg_send(uint8_t message_id, uint8_t priority);

/** Send message quickly by callback handler given msgnum, start of the search
 *
 * \param isn_msg_handler_t
 * \param priority defines order in which messages are sent out.
 *        Setting MSB bit 0x80 will request to send out descriptors instead of a data
 * \param returns message_id on success otherwise 0xFF
 */
uint8_t isn_msg_sendqby(isn_events_handler_t hnd, uint8_t priority, uint8_t msgnum);

/** Send message by callback handler
 *
 * \param isn_msg_handler_t
 * \param priority defines order in which messages are sent out.
 *        Setting MSB bit 0x80 will request to send out descriptors instead of a data
 * \param returns message_id on success otherwise 0xFF
 */
static inline uint8_t isn_msg_sendby(isn_events_handler_t hnd, uint8_t priority) {return isn_msg_sendqby(hnd,priority,0);}

/**
 * Callback may call this function to confirm that input relates
 * to a callback argument from message layer.
 * 
 * Useful if same callback event is called from multiple sources.
 * 
 * \returns Non-zero if valid and argument is non-zero, otherwise 0
 */
int isn_msg_isinput_valid(const void *arg);

/**
 * To be used within the callback, it may ask with which priority
 * was it called to be able to distinguish also from external
 * (HIGHEST) queries and internal ones
 */
static inline int isn_msg_isquery() {return handler_priority == ISN_MSG_PRI_HIGHEST;}

#endif
