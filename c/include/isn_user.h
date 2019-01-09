/** \file
 *  \see https://www.isotel.eu/isn/user.html
 */

#ifndef __ISN_USER_H__
#define __ISN_USER_H__

#include "isn.h"

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* parent;
    isn_driver_t* child;

    uint8_t user_id;
}
isn_user_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** User Layer
 * 
 * \param child layer
 * \param parent protocol layer, which is typically a PHY, or FRAME
 * \param user_id user id from ISN_PROTO_USERx
 */
void isn_user_init(isn_user_t *obj, isn_layer_t* child, isn_layer_t* parent, uint8_t user_id);

#endif
