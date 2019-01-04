/** \file
 */

#ifndef __ISN_USBUART_H__
#define __ISN_USBUART_H__

#include "isn.h"

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** ISN Layer Driver */
extern isn_layer_t isn_usbuart;

/** Polls for a new data received from PC and dispatch them 
 * \returns number of bytes received
 */
size_t isn_usbuart_poll();

/** Initialize
 * 
 * \param mode USBUART_3V_OPERATION
 * \param child use the next layer, like isn_frame
 */
void isn_usbuart_init(int mode, isn_layer_t* child);

#endif
