/*
   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/

#ifndef CHAN_QUECTEL_SMSDB_H_INCLUDED
#define CHAN_QUECTEL_SMSDB_H_INCLUDED

#define SMSDB_PAYLOAD_MAX_LEN 4096
#define SMSDB_DST_MAX_LEN 256

int smsdb_init();
void smsdb_atexit();
int smsdb_put(const char *id, const char *addr, int ref, int parts, int order, const char *msg, char *out);
int smsdb_get_refid(const char *id, const char *addr);
int smsdb_outgoing_add(const char *id, const char *addr, int cnt, int ttl, int srr, const char *payload, size_t len);
ssize_t smsdb_outgoing_clear(int uid, char *dst, char *payload);
ssize_t smsdb_outgoing_part_put(int uid, int refid, char *dst, char *payload);
ssize_t smsdb_outgoing_part_status(const char *id, const char *addr, int mr, int st, int *status_all, char *payload);
ssize_t smsdb_outgoing_purge_one(char *dst, char *payload);

#endif
