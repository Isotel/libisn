#include "isn_user.h"

int isn_user_getsendbuf(isn_layer_t *drv, uint8_t **buf, size_t size) {
    isn_user_t *obj = (isn_user_t *)drv;
    int osize = obj->parent->getsendbuf(obj->parent, buf, size+1);
    if (buf) {
        if (*buf) (*buf)++; // add protocol header at the front
    }
    return osize-1;
}

void isn_user_free(isn_layer_t *drv, const uint8_t *buf) {
    isn_user_t *obj = (isn_user_t *)drv;
    if (buf) obj->parent->free(obj->parent, buf-1);
}

int isn_user_send(isn_layer_t *drv, uint8_t *buf, size_t size) {
    isn_user_t *obj = (isn_user_t *)drv;
    *(--buf) = obj->user_id;
    obj->parent->send(obj->parent, buf, size+1);
    return 0;
}

const uint8_t * isn_user_recv(isn_layer_t *drv, const uint8_t *buf, size_t size, isn_driver_t *caller) {
    isn_user_t *obj = (isn_user_t *)drv;
    return obj->child->recv(obj->child, buf+1, size-1, drv);
}

void isn_user_init(isn_user_t *obj, isn_layer_t* child, isn_layer_t* parent, uint8_t user_id) {
    obj->drv.getsendbuf = isn_user_getsendbuf;
    obj->drv.send       = isn_user_send;
    obj->drv.recv       = isn_user_recv;
    obj->drv.free       = isn_user_free;
    obj->user_id        = user_id;
    obj->child          = child;
    obj->parent         = parent;
}
