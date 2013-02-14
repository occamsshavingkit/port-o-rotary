#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#define CBUF_OVERFLOW       0x01
#define CBUF_UNDERFLOW      0x02

typedef struct {
    unsigned int size;
    unsigned int head;
    unsigned int tail;
    unsigned char error;
    char *elements;
} cbuf;

void cbuf_init(cbuf *cb, int size, char *elements);
char cbuf_is_full(cbuf *cb);
char cbuf_is_empty(cbuf *cb);
int cbuf_incr(cbuf *cb, int p);
char cbuf_get(cbuf *cb);
char cbuf_put(cbuf *cb, char c);
char cbuf_push(cbuf *cb, char c);
char cbuf_pop(cbuf *cb);

#endif