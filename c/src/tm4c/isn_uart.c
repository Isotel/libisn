/** \file
 *  \author Nelson Gaston Sanchez <gaston.sanchez@dewesoft.com>
 *  \see isn_uart.h
 * 
 * \addtogroup GR_ISN_PSoC_UART
 * 
 * # Tested
 * 
 *  - Families: TM4C
 * 
 * \cond Implementation
 */

#include "isn_uart.h"

extern isn_uart_t     isn_uart_host,isn_uart_debug;
static isn_uart_t *objs[2];

#define UART_uDMA
/**\{ */

unsigned char UART_PutArray(uint8_t* dest,uint8_t size,uint32_t base,uint32_t uDMAbase){

#ifdef UART_uDMA
   uDMAChannelTransferSet(uDMAbase | UDMA_PRI_SELECT, UDMA_MODE_BASIC, dest, (void *) (base + UART_O_DR),size);
   uDMAChannelEnable(uDMAbase);
   UARTDMAEnable(base, UART_DMA_TX);
   return size;
#else
    uint8_t i;
    uint8_t *destination = dest;
    for(i=0 ; i<size ; i++){
        UARTCharPutNonBlocking(base,*destination);
        while(UARTBusy(base));
        destination++;
    }
    return i;
#endif
}

/**
 * Allocate buffer if buf is given, or just query for availability if buf is NULL
 * 
 * \returns desired or limited (max) size in the case desired size is too big
 */
static int isn_uart_getsendbuf(isn_layer_t *drv, void **dest, size_t size, isn_layer_t *caller) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    if ((!uDMAChannelIsEnabled(obj->DMAtx)) && !obj->buf_locked) {
        if (dest) {
            obj->buf_locked = 1;
            *dest = obj->txbuf;
        }
        return (size > UART_TXBUF_SIZE) ? UART_TXBUF_SIZE : size;
    }
    if (dest) {
        *dest = NULL;
    }
    return -1;
}

static void isn_uart_free(isn_layer_t *drv, const void *ptr) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    if (ptr == obj->txbuf) {
        obj->buf_locked = 0;             // we only support one buffer so we may free
    }
}

static int isn_uart_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_uart_t *obj = (isn_uart_t *)drv;
    assert(size <= TXBUF_SIZE);
    while (uDMAChannelIsEnabled(obj->DMAtx));
    UART_PutArray(dest,size,obj->base,obj->DMAtx);
    isn_uart_free(drv, dest);           // free buffer, however need to block use of buffer until sent out
    return size;
}

size_t isn_uart_poll(isn_uart_t *obj) {
    size_t size = 0;
    if (obj->rx_size) {
        size = obj->child_driver->recv(obj->child_driver, obj->rxbuf, obj->rx_size, &obj->drv);
        IntDisable(obj->intnum);
        if (size < obj->rx_size) {
            obj->rx_retry++;    // Packet could not be fully accepted, retry next time
            memmove(obj->rxbuf, &obj->rxbuf[size], obj->rx_size - size);
        }
        obj->rx_size -= size;
        IntEnable(obj->intnum);
    }
    return size;
}

void UART0_Handler(void) {
    uint32_t status = UARTIntStatus(UART0_BASE,0);
    UARTIntClear(UART0_BASE,status);
    HWREG(UART0_BASE + UART_O_DR) = '*';
    if (status & (UART_INT_FE|UART_INT_PE|UART_INT_BE|UART_INT_OE)) {
        UARTRxErrorClear(UART0_BASE);
        while (UARTCharsAvail(UART0_BASE)) {
            UARTCharGetNonBlocking(UART0_BASE);
        }
        objs[0]->rx_error++;
    }
    else if (status & (UART_INT_RX|UART_INT_RT) ) {
        while (UARTCharsAvail(UART0_BASE) && objs[0]->rx_size < UART_RXBUF_SIZE) {
            objs[0]->rxbuf[objs[0]->rx_size++] = UARTCharGetNonBlocking(UART0_BASE);
        }
    }
}

void UART1_Handler(void) {
    uint32_t status = UARTIntStatus(UART1_BASE,0);
    UARTIntClear(UART1_BASE,status);
    if (status & (UART_INT_FE|UART_INT_PE|UART_INT_BE|UART_INT_OE)) {
        UARTRxErrorClear(UART1_BASE);
        while (UARTCharsAvail(UART1_BASE)) {
            UARTCharGetNonBlocking(UART1_BASE);
        }
        objs[1]->rx_error++;
    }
    else if (status & (UART_INT_RX|UART_INT_RT) ) {
        while (UARTCharsAvail(UART1_BASE) && objs[1]->rx_size < UART_RXBUF_SIZE) {
            objs[1]->rxbuf[objs[1]->rx_size++] = UARTCharGetNonBlocking(UART1_BASE);
        }
    }
}

void isn_uart_init(isn_uart_t *obj, isn_layer_t* child, uint8_t port) {
    uint32_t status;
    obj->drv.getsendbuf = isn_uart_getsendbuf;
    obj->drv.send = isn_uart_send;
    obj->drv.recv = NULL;
    obj->drv.free = isn_uart_free;
    obj->child_driver = child;
    obj->buf_locked = 0;

    switch (port) {
        case 0:
            obj->base = UART0_BASE;
            obj->intnum = INT_UART0;
            obj->DMArx = UDMA_CH8_UART0RX;
            obj->DMAtx = UDMA_CH9_UART0TX;
            objs[0] = obj;
            ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
            ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
            ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
            ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
            ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
            break;
        case 1:
            obj->base = UART1_BASE;
            obj->intnum = INT_UART1;
            obj->DMArx = UDMA_CH22_UART1RX;
            obj->DMAtx = UDMA_CH23_UART1TX;
            objs[1] = obj;
            SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
            SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
            GPIOPinConfigure(GPIO_PB0_U1RX);
            GPIOPinConfigure(GPIO_PB1_U1TX);
            GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
            break;
    }

    UARTClockSourceSet(obj->base, UART_CLOCK_PIOSC);
    UARTFIFOLevelSet(obj->base, UART_FIFO_TX7_8, UART_FIFO_RX6_8);
    UARTFIFOEnable(obj->base);
    UARTEnable(obj->base);
    UARTConfigSetExpClk(obj->base, 16000000, PSOC_UART_BAUDRATE,
                      (UART_CONFIG_PAR_EVEN | UART_CONFIG_STOP_TWO |
                       UART_CONFIG_WLEN_8));
    status = UARTIntStatus(obj->base,0);
    UARTIntClear(obj->base,status);
    UARTRxErrorClear(obj->base);
    IntPrioritySet(obj->intnum, 0xE0);
    UARTIntEnable(obj->base,UART_INT_RT|UART_INT_FE|UART_INT_PE|UART_INT_BE|UART_INT_OE|UART_INT_RX);
    IntEnable(obj->intnum);

    uDMAChannelAssign(obj->DMArx);
    uDMAChannelDisable(obj->DMArx);
    uDMAChannelAttributeDisable(obj->DMArx, UDMA_ATTR_ALTSELECT | UDMA_ATTR_USEBURST | UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK);
    uDMAChannelControlSet(obj->DMArx | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_NONE | UDMA_DST_INC_8 | UDMA_ARB_4);

    uDMAChannelAssign(obj->DMAtx);
    uDMAChannelDisable(obj->DMAtx);
    uDMAChannelAttributeDisable(obj->DMAtx, UDMA_ATTR_ALTSELECT | UDMA_ATTR_USEBURST | UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK);
    uDMAChannelControlSet(obj->DMAtx | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_4);

}

/** \} \endcond */
