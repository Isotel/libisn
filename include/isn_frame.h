/** \file
 *  \see https://www.isotel.eu/isn/frame.html
 */

#ifndef __ISN_FRAME_H__
#define __ISN_FRAME_H__

#include "isn.h"

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

typedef enum {
    ISN_FRAME_MODE_SHORT    = 0,    // 1-byte overhead (header)
    ISN_FRAME_MODE_COMPACT  = 1     // 2-bytes overhead (header + 8-bit crc)
} isn_frame_mode_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** ISN Layer Driver */
extern isn_layer_t isn_frame;


/** Short and Compact Frame Layer
 * 
 * \param mode selects short (without CRC) or compact (with CRC) which is typically used over noisy lines, as UART
 * \param bindings provide a map of possible child protocols, and terminate with the PROTOCOL_OTHERWISE which provides 
 *        a bypass to raw terminal
 * \param parent protocol layer, which is typically a PHY, or UART or USBUART, ..
 * \param counter a pointer to a free running counter at arbitrary frequency
 * \param timeout defines period with reference to the counter after which reception is treated as invalid and to be discarded
 *        A 100 ms is a good choice.
 */
void isn_frame_init(isn_frame_mode_t mode, isn_bindings_t* bindings, isn_layer_t* parent, volatile uint32_t *counter, uint32_t timeout);

#endif
