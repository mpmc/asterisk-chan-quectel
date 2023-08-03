/*
   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/
#ifndef CHAN_QUECTEL_CHAR_CONV_H_INCLUDED
#define CHAN_QUECTEL_CHAR_CONV_H_INCLUDED

#include <stdint.h>
#include <sys/types.h> /* ssize_t size_t */

ssize_t utf8_to_ucs2(const char* in, size_t in_length, uint16_t* out, size_t out_size);
ssize_t ucs2_to_utf8(const uint16_t* in, size_t in_length, char* out, size_t out_size);
int unhex(const char* in, uint8_t* out);
void hexify(const uint8_t* in, size_t in_length, char* out);
ssize_t gsm7_encode(const uint16_t* in, size_t in_length, uint16_t* out);
ssize_t gsm7_pack(const uint16_t* in, size_t in_length, char* out, size_t out_size, unsigned out_padding);
ssize_t gsm7_unpack_decode(const char* in, size_t in_length, uint16_t* out, size_t out_size, unsigned in_padding, uint8_t ls, uint8_t ss);

#endif /* CHAN_QUECTEL_CHAR_CONV_H_INCLUDED */
