#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "isn.h"

typedef struct {
    isn_driver_t drv;
    isn_driver_t *child;
}
isn_tester_t;

isn_tester_t tester;
isn_frame_jumbo_t frame;
int success = -1;

static int tester_getsendbuf(isn_layer_t *drv, void **dest, size_t size, const isn_layer_t *caller) {
    isn_tester_t *obj = (isn_tester_t *)drv;
    printf("tester_getsendbuf: %ld\n", size);
    if (dest) {
        *dest = malloc(size);
        return *dest ? size : 0;
    }
    return size;
}

static void tester_free(isn_layer_t *drv, const void *ptr) {
    free((void *)ptr);
}

static int tester_send(isn_layer_t *drv, void *dest, size_t size) {
    isn_tester_t *obj = (isn_tester_t *)drv;
    uint8_t *b = (uint8_t *)dest;
    for (int i=0; i<size; i++) printf("%.2x ", b[i]);
    printf("tester_send and returning back: %ld\n", size);
    obj->child->recv(obj->child, dest, size, obj);
    free(dest);
    return size;
}

void isn_tester_init(isn_tester_t *obj, isn_layer_t* child) {
    ASSERT(obj);
    ASSERT(child);
    memset(&obj->drv, 0, sizeof(obj->drv));

    obj->drv.getsendbuf   = tester_getsendbuf;
    obj->drv.send         = tester_send;
    obj->drv.recv         = NULL;
    obj->drv.free         = tester_free;
    obj->child            = child;
}

size_t other_recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    uint8_t *b = (uint8_t *)src;
    for (int i=0; i<size; i++) printf("%.2x ", b[i]);
    printf("other: %ld\n", size);
    return size;    // Ack entire packet
}

size_t recv(isn_layer_t *drv, const void *src, size_t size, isn_layer_t *caller) {
    uint8_t *b = (uint8_t *)src;
    for (int i=0; i<size; i++) printf("%.2x ", b[i]);
    printf("recv: %ld\n", size);
    success = 0;
    return size;    // Ack entire packet
}

int main(int argc, char *argv[]) {
    isn_tester_init(&tester, &frame);
    isn_frame_jumbo_init(&frame, &(isn_receiver_t){recv}, &(isn_receiver_t){other_recv}, &tester, ISN_CLOCK_ms(10));

    //gen_32crc();

    isn_write(&frame, "test", 4);

    return success;
}