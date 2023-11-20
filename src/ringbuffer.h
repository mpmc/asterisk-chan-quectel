/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>
*/

#ifndef ____RINGBUFFER_H__
#define ____RINGBUFFER_H__

#include <sys/uio.h> /* struct iovec */

typedef void* (*rb_write_f)(void* s1, const void* s2, size_t n);

struct ringbuffer {
    void* buffer; /*!< pointer to data buffer */
    size_t size;  /*!< size of buffer */
    size_t used;  /*!< number of bytes used */
    size_t read;  /*!< read position */
    size_t write; /*!< write position */
};

static inline void rb_init(struct ringbuffer* rb, void* buf, size_t size)
{
    rb->buffer = buf;
    rb->size   = size;
    rb->used   = 0;
    rb->read   = 0;
    rb->write  = 0;
}

static inline void rb_reset(struct ringbuffer* const rb)
{
    rb->used  = 0;
    rb->read  = 0;
    rb->write = 0;
}

static inline size_t rb_used(const struct ringbuffer* rb) { return rb->used; }

static inline size_t rb_free(const struct ringbuffer* rb) { return rb->size - rb->used; }

int rb_memcmp(const struct ringbuffer*, const char*, size_t);

/*!< fill io vectors array with read data (situable for writev()) and return number of io vectors updated  */
int rb_read_all_iov(const struct ringbuffer* rb, struct iovec* iov);

/*!< fill io vectors array and return number of io vectors updated for reading len bytes */
int rb_read_n_iov(const struct ringbuffer* rb, struct iovec* iov, size_t len);

int rb_read_until_char_iov(const struct ringbuffer*, struct iovec* iov, char);
int rb_read_until_mem_iov(const struct ringbuffer*, struct iovec* iov, const void*, size_t);

/*!< advice read position to len bytes */
size_t rb_read_upd(struct ringbuffer* rb, size_t len);

int rb_read_is_printable(struct ringbuffer* rb);

/*!< fill io vectors array with free data (situable for readv()) and return number of io vectors updated  */
int rb_write_iov(const struct ringbuffer*, struct iovec* iov);

/*!< advice write position to len bytes */
size_t rb_write_upd(struct ringbuffer*, size_t);

size_t rb_write_core(struct ringbuffer* rb, const char* buf, size_t len, rb_write_f method);

static inline size_t rb_write(struct ringbuffer* rb, const char* buf, size_t len) { return rb_write_core(rb, buf, len, memmove); }

#endif /* ____RINGBUFFER_H__ */
