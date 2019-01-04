/** \file
 *  \see https://www.isotel.eu/isn
 */

#ifndef __ISN_H__
#define __ISN_H__

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define ISN_PROTO_FRAME     0x80
#define ISN_PROTO_MSG       0x7F
#define ISN_PROTO_TRANS     0x7E
#define ISN_PROTO_TRANL     0x7D
#define ISN_PROTO_USER7     0x07
#define ISN_PROTO_USER6     0x06
#define ISN_PROTO_USER5     0x05
#define ISN_PROTO_USER4     0x04
#define ISN_PROTO_USER3     0x03
#define ISN_PROTO_USER2     0x02
#define ISN_PROTO_USER1     0x01
#define ISN_PROTO_PING      0x00
#define ISN_PROTO_OTHERWISE -1

/**
 * ISN Layer (Driver)
 */
typedef struct isn_layer_s {
    int             (*getsendbuf)(uint8_t **buf, size_t size);  // Returns point to allocated buffer to be passed to send()
    int             (*send)(uint8_t *buf, size_t size);         // Returns number of bytes sent or negative on error
    const uint8_t * (*recv)(const uint8_t *buf, size_t size, struct isn_layer_s *caller); // Returns the same receive pointer to free it, or NULL if it is not ready to be freed
    void            (*free)(const uint8_t *buf);                // Used to free a buffer provided by recv() which returned NULL
} isn_layer_t;

/**
 * ISN Protocol to Layer Drivers Bindings
 */
typedef struct {
    int protocol;
    isn_layer_t *driver;
} isn_bindings_t;

/**
 * Callback event handler
 */
typedef void* (* isn_events_handler_t)(const void* arg);

/* Helpers */
#define LAMBDA(c_) ({ c_ _;})

#endif
