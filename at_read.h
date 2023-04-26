/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_QUECTEL_AT_READ_H_INCLUDED
#define CHAN_QUECTEL_AT_READ_H_INCLUDED

#include "at_response.h"		/* at_res_t */

struct pvt;
struct ringbuffer;
struct iovec;

int at_wait (int fd, int* ms);
ssize_t at_read (int fd, const char * dev, struct ringbuffer* rb);
int at_read_result_iov (const char * dev, int * read_result, struct ringbuffer* rb, struct iovec * iov);
at_res_t at_read_result_classification (struct ringbuffer * rb, size_t len);

#endif /* CHAN_QUECTEL_AT_READ_H_INCLUDED */
