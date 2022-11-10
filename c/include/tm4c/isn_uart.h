/** \file
 *  \author Nelson Gaston Sanchez <gaston.sanchez@dewesoft.com>
 *  \see isn_uart.c
 */
/**
 * \ingroup GR_ISN_TM4C
 * \defgroup GR_ISN_TM4C_UART TM4C UART Driver
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


#include "isn.h"

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
    uint8_t rxbuf_aux[UART_RXBUF_SIZE];
    int buf_locked;
    uint8_t rx_size;
    uint8_t rx_size_aux;
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
 * \param obj
 * \param child use the next layer, like isn_frame
 * \param port TM4C UART number
 */
void isn_uart_init(isn_uart_t *obj, isn_layer_t* child, uint8_t port);
unsigned char UART_PutArray(uint8_t* dest,uint8_t size,uint32_t base, uint32_t uDMAbase);
unsigned char UART_SpiUartReadRxData(void);
void ConfigureUART(uint8_t hwVersionMotherboard);

#endif
