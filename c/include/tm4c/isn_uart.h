/** \file
 *  \author Nelson Gaston Sanchez <gaston.sanchez@dewesoft.com>
 *  \see isn_uart.c
 * 
 * \defgroup GR_ISN_PSoC_UART ISN Driver for PSoC UART
 * 
 * # Scope
 * 
 * Tiny implementation of the ISN Device Driver for the 
 * TM4C UART and supports non-blocking and blocking mode.
 * 
 * # Usage
 * 
 * 
 * - if the TX buffer is below 64 bytes, device may operate in blocking mode, if
 *   desired packet cannot fit the hardware fifo,
 * - otherwise device driver operates in non-blocking mode.
 * 
 */


#ifndef __ISN_UART_H__
#define __ISN_UART_H__


#include "../isn.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/rom.h"
#include "utils/uartstdio.h"
#include "string.h"
#include "inc/hw_uart.h"
#include "inc/hw_types.h"
#include "driverlib/udma.h"

#define PSOC_UART_BAUDRATE   115200   // default UART baudrate

#define UART_TXBUF_SIZE  (64)
#define UART_RXBUF_SIZE  (64)

#define TXBUF_SIZE  (64)
#define RXBUF_SIZE  (64)

/** ISN Layer Driver */
typedef struct {
    /* ISN Abstract Class Driver */
    isn_driver_t drv;

    /* Private data */
    isn_driver_t* child_driver;
    uint8_t txbuf[UART_TXBUF_SIZE];
    uint8_t rxbuf[UART_RXBUF_SIZE];
    int buf_locked;
    uint8_t rx_size;
    size_t rx_retry;
    size_t rx_error;
    uint32_t base;
    uint32_t intnum;
    uint32_t DMArx;
    uint32_t DMAtx;
}
isn_uart_t;

/*----------------------------------------------------------------------*/
/* Public functions                                                     */
/*----------------------------------------------------------------------*/

/** Polls for a new data received from PC and dispatch them 
 * \returns number of bytes received
 */
size_t isn_uart_poll(isn_uart_t *obj);

/** Initialize
 * 
 * \param child use the next layer, like isn_frame
 */
void isn_uart_init(isn_uart_t *obj, isn_layer_t* child, uint8_t port);
unsigned char UART_PutArray(uint8_t* dest,uint8_t size,uint32_t base, uint32_t uDMAbase);
unsigned char UART_SpiUartReadRxData(void );

#endif
