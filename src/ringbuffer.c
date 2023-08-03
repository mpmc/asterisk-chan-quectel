/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>
*/
#include <string.h> /* memchr() */

#include "ast_config.h"

#include "ringbuffer.h"

#include "memmem.h"

int rb_memcmp(const struct ringbuffer* rb, const char* mem, size_t len)
{
    size_t tmp;

    if (rb->used > 0 && len > 0 && rb->used >= len) {
        if ((rb->read + len) > rb->size) {
            tmp = rb->size - rb->read;
            if (memcmp(rb->buffer + rb->read, mem, tmp) == 0) {
                len -= tmp;
                mem += tmp;

                if (memcmp(rb->buffer, mem, len) == 0) {
                    return 0;
                }
            }
        } else {
            if (memcmp(rb->buffer + rb->read, mem, len) == 0) {
                return 0;
            }
        }

        return 1;
    }

    return -1;
}

/* ============================ READ ============================= */

int rb_read_all_iov(const struct ringbuffer* rb, struct iovec* iov)
{
    if (rb->used > 0) {
        if ((rb->read + rb->used) > rb->size) {
            iov[0].iov_base = rb->buffer + rb->read;
            iov[0].iov_len  = rb->size - rb->read;
            iov[1].iov_base = rb->buffer;
            iov[1].iov_len  = rb->used - iov[0].iov_len;
            return 2;
        } else {
            iov[0].iov_base = rb->buffer + rb->read;
            iov[0].iov_len  = rb->used;
            iov[1].iov_len  = 0;
            return 1;
        }
    }

    return 0;
}

int rb_read_n_iov(const struct ringbuffer* rb, struct iovec* iov, size_t len)
{
    if (rb->used < len) {
        return 0;
    }

    if (len > 0) {
        if ((rb->read + len) > rb->size) {
            iov[0].iov_base = rb->buffer + rb->read;
            iov[0].iov_len  = rb->size - rb->read;
            iov[1].iov_base = rb->buffer;
            iov[1].iov_len  = len - iov[0].iov_len;
            return 2;
        } else {
            iov[0].iov_base = rb->buffer + rb->read;
            iov[0].iov_len  = len;
            iov[1].iov_len  = 0;
            return 1;
        }
    }

    return 0;
}

int rb_read_until_char_iov(const struct ringbuffer* rb, struct iovec* iov, char c)
{
    if (rb->used > 0) {
        void* p;

        if ((rb->read + rb->used) > rb->size) {
            iov[0].iov_base = rb->buffer + rb->read;
            iov[0].iov_len  = rb->size - rb->read;
            if ((p = memchr(iov[0].iov_base, c, iov[0].iov_len)) != NULL) {
                iov[0].iov_len = p - iov[0].iov_base;
                iov[1].iov_len = 0;
                return 1;
            }

            if ((p = memchr(rb->buffer, c, rb->used - iov[0].iov_len)) != NULL) {
                iov[1].iov_base = rb->buffer;
                iov[1].iov_len  = p - rb->buffer;
                return 2;
            }
        } else {
            iov[0].iov_base = rb->buffer + rb->read;
            iov[0].iov_len  = rb->used;
            if ((p = memchr(iov[0].iov_base, c, iov[0].iov_len)) != NULL) {
                iov[0].iov_len = p - iov[0].iov_base;
                iov[1].iov_len = 0;
                return 1;
            }
        }
    }

    return 0;
}

int rb_read_until_mem_iov(const struct ringbuffer* rb, struct iovec* iov, const void* mem, size_t len)
{
    if (len == 1) {
        return rb_read_until_char_iov(rb, iov, *((char*)mem));
    }

    if (rb->used > 0 && len > 0 && rb->used >= len) {
        size_t i;
        void* p;

        if ((rb->read + rb->used) > rb->size) {
            iov[0].iov_base = rb->buffer + rb->read;
            iov[0].iov_len  = rb->size - rb->read;
            if (iov[0].iov_len >= len) {
                if ((p = memmem(iov[0].iov_base, iov[0].iov_len, mem, len)) != NULL) {
                    iov[0].iov_len = p - iov[0].iov_base;
                    iov[1].iov_len = 0;
                    return 1;
                }

                i               = 1;
                iov[1].iov_base = iov[0].iov_base + iov[0].iov_len - len + 1;
            } else {
                i               = len - iov[0].iov_len;
                iov[1].iov_base = iov[0].iov_base;
            }

            while (i < len) {
                if (memcmp(iov[1].iov_base, mem, len - i) == 0 && memcmp(rb->buffer, mem + i, i) == 0) {
                    iov[0].iov_len = iov[1].iov_base - iov[0].iov_base;
                    iov[1].iov_len = 0;
                    return 1;
                }

                if (rb->used == iov[0].iov_len + i) {
                    return 0;
                }

                iov[1].iov_base++;
                i++;
            }

            if (rb->used >= iov[0].iov_len + len) {
                if ((p = memmem(rb->buffer, rb->used - iov[0].iov_len, mem, len)) != NULL) {
                    if (p == rb->buffer) {
                        iov[1].iov_len = 0;
                        return 1;
                    }

                    iov[1].iov_base = rb->buffer;
                    iov[1].iov_len  = p - rb->buffer;
                    return 2;
                }
            }
        } else {
            iov[0].iov_base = rb->buffer + rb->read;
            iov[0].iov_len  = rb->used;
            if ((p = memmem(iov[0].iov_base, iov[0].iov_len, mem, len)) != NULL) {
                iov[0].iov_len = p - iov[0].iov_base;
                iov[1].iov_len = 0;

                return 1;
            }
        }
    }

    return 0;
}

size_t rb_read_upd(struct ringbuffer* rb, size_t len)
{
    size_t s;

    if (rb->used < len) {
        len = rb->used;
    }

    if (len > 0) {
        rb->used -= len;

        if (rb->used == 0) {
            rb->read  = 0;
            rb->write = 0;
        } else {
            s = rb->read + len;

            if (s >= rb->size) {
                rb->read = s - rb->size;
            } else {
                rb->read = s;
            }
        }
    }

    return len;
}

/* ============================ WRITE ============================ */

int rb_write_iov(const struct ringbuffer* rb, struct iovec* iov)
{
    const size_t free = rb_free(rb);

    if (free > 0) {
        if ((rb->write + free) > rb->size) {
            iov[0].iov_base = rb->buffer + rb->write;
            iov[0].iov_len  = rb->size - rb->write;
            iov[1].iov_base = rb->buffer;
            iov[1].iov_len  = free - iov[0].iov_len;

            return 2;
        } else {
            iov[0].iov_base = rb->buffer + rb->write;
            iov[0].iov_len  = free;

            return 1;
        }
    }

    return 0;
}

size_t rb_write_upd(struct ringbuffer* rb, size_t len)
{
    const size_t free = rb_free(rb);

    if (free < len) {
        len = free;
    }

    if (len > 0) {
        const size_t s = rb->write + len;

        if (s > rb->size) {
            rb->write = s - rb->size;
        } else {
            rb->write = s;
        }

        rb->used += len;
    }

    return len;
}

size_t rb_write_core(struct ringbuffer* rb, const char* buf, size_t len, rb_write_f method)
{
    const size_t free = rb_free(rb);
    if (free < len) {
        len = free;
    }

    if (len > 0) {
        const size_t s = rb->write + len;

        if (s > rb->size) {
            (*method)(rb->buffer + rb->write, buf, rb->size - rb->write);
            (*method)(rb->buffer, buf + rb->size - rb->write, s - rb->size);
            rb->write = s - rb->size;
        } else {
            (*method)(rb->buffer + rb->write, buf, len);
            if (s == rb->size) {
                rb->write = 0;
            } else {
                rb->write = s;
            }
        }

        rb->used += len;
    }

    return len;
}
