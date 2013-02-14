#include "circular_buffer.h"

void cbuf_init(cbuf *buf, int size, char *elements){
    buf->size = size;
    buf->head = 0;
    buf->tail = 0;
    buf->error = 0;
    buf->elements = elements;
}

void cbuf_print(cbuf *cb) {
    printf("size=%d, head=%d, tail=%d\n", cb->size, cb->head, cb->tail);
}

char cbuf_is_full(cbuf *cb){
    return cb->tail == (cb->head ^ cb->size); 
    /* This inverts the most significant bit of head before comparison */
}

char cbuf_is_empty(cbuf *cb){
    return cb->tail == cb->head;   
}

int cbuf_incr(cbuf *cb, int p){
    return (p + 1) & (2 * cb->size - 1); 
    /* head and tail pointers incrementation is done modulo 2*size */
}

int cbuf_decr(cbuf *cb, int p){
    if (p > 0) {
        return (p - 1); 
    }
    else {
        return 2*cb->size - 1;
    }
    /* head and tail pointers decrementation is done modulo 2*size */
    
}

void cbuf_clear_error(cbuf *cb, char errors){
    cb->error &= ~(errors);
}

char cbuf_put(cbuf *cb, char c){
    if (cbuf_is_full(cb)){ /* full, overwrite moves head pointer */
        cb->head = cbuf_incr(cb, cb->head);
        cb->error |= CBUF_OVERFLOW;
    }
    cb->elements[cb->tail&(cb->size-1)] = c;
    cb->tail = cbuf_incr(cb, cb->tail);
    return cb->error;
}

char cbuf_get(cbuf *cb) {
    if(cbuf_is_empty(cb)){
        cb->error |= CBUF_UNDERFLOW;
        return 0;
    }
    char c = cb->elements[cb->head&(cb->size-1)];
    cb->head = cbuf_incr(cb, cb->head);
    return c;
}

char cbuf_push(cbuf *cb, char c){
    cb->elements[cb->tail&(cb->size-1)] = c;
    if (cbuf_is_full(cb)){ /* full, overwrite moves head pointer */
        cb->head = cbuf_decr(cb, cb->head);
        cb->error |= CBUF_OVERFLOW;
    }
    cb->tail = cbuf_decr(cb, cb->tail);
    return cb->error;

}
char cbuf_pop(cbuf *cb){
    return cbuf_get(cb);
}