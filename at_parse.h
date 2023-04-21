/*
    Copyright (C) 2010 bg <bg_one@mail.ru>
    Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/
#ifndef CHAN_QUECTEL_AT_PARSE_H_INCLUDED
#define CHAN_QUECTEL_AT_PARSE_H_INCLUDED

#include <sys/types.h>			/* size_t */

#include "char_conv.h"		/* str_encoding_t */
#include "pdu.h"
struct pvt;

typedef enum
{
    QIND_NONE = 0,
    QIND_CSQ,
    QIND_ACT,
    QIND_CCINFO
} qind_t;

char* at_parse_cnum(char* str);
char* at_parse_cops(char* str);
int at_parse_creg(char* str, unsigned len, int* gsm_reg, int* gsm_reg_status, char** lac, char** ci);
int at_parse_cmti(const char* str);
int at_parse_cdsi(const char* str);
int at_parse_cmgr(char *str, size_t len, int *tpdu_type, char *sca, size_t sca_len, char *oa, size_t oa_len, char *scts, int *mr, int *st, char *dt, char *msg, size_t *msg_len, pdu_udh_t *udh);
int at_parse_cmgs(const char* str);
int at_parse_cusd(char* str, int * type, char ** cusd, int * dcs);
int at_parse_cpin(char* str, size_t len);
int at_parse_csq(const char* str, int* rssi);
int at_parse_rssi(const char* str);
int at_parse_qind(char* str, qind_t* qind, char** params);
int at_parse_qind_csq(const char* params, int* rssi);
int at_parse_qind_act(char* params, int* act);
int at_parse_qind_cc(char* params, unsigned* call_idx, unsigned* dir, unsigned* state, unsigned* mode, unsigned* mpty, char** number, unsigned* toa);
int at_parse_csca(char* str, char ** csca);
int at_parse_dsci(char* str, unsigned* call_idx, unsigned* dir, unsigned* state, unsigned* call_type, char** number, unsigned* toa);
int at_parse_clcc(char* str, unsigned* call_idx, unsigned* dir, unsigned* state, unsigned* mode, unsigned* mpty, char** number, unsigned* toa);
int at_parse_ccwa(char* str, unsigned * class);

#endif /* CHAN_QUECTEL_AT_PARSE_H_INCLUDED */
