/** \file
 */

#ifndef __ISN_USBUART_H__
#define __ISN_USBUART_H__

#include "isn.h"

#define TXBUF_SIZE  64
#define RXBUF_SIZE  64

/** ISN Layer Driver */
typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* child_driver;
    uint8_t txbuf[TXBUF_SIZE];
    uint8_t rxbuf[RXBUF_SIZE];
    int buf_locked;
}
isn_usbuart_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Polls for a new data received from PC and dispatch them 
 * \returns number of bytes received
 */
size_t isn_usbuart_poll(isn_usbuart_t *obj);

/** Initialize
 * 
 * \param mode USBUART_3V_OPERATION
 * \param child use the next layer, like isn_frame
 */
void isn_usbuart_init(isn_usbuart_t *obj, int mode, isn_layer_t* child);

#endif
