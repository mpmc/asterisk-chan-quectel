/*
   Copyright (C) 2010,2011 bg <bg_one@mail.ru>
*/
#ifndef CHAN_QUECTEL_HELPERS_H_INCLUDED
#define CHAN_QUECTEL_HELPERS_H_INCLUDED

#include <asterisk/strings.h>

#include "chan_quectel.h" /* restate_time_t */
#include "dc_config.h"    /* call_waiting_t */

int get_at_clir_value(struct pvt* pvt, int clir);

/* return status string of sending, status arg is optional */
int send_ussd(const char* dev_name, const char* ussd);
int send_sms(const char* const dev_name, const char* const number, const char* const message, int validity, int report, const char* const payload,
             size_t payload_len);
int list_sms(const char* const dev_name, enum msg_status_t stat);
int delete_sms(const char* const dev_name, unsigned int idx, int delflag);
int sms_direct(const char* const dev_name, int directflag);
int send_reset(const char* dev_name);
int send_ccwa_set(const char* dev_name, call_waiting_t enable);
int query_qaudloop(const char* dev_name);
int send_qaudloop(const char* dev_name, int aloop);
int query_qaudmod(const char* dev_name);
int send_qaudmod(const char* dev_name, int amode);
int query_micgain(const char* dev_name);
int send_micgain(const char* dev_name, int gain);
int query_rxgain(const char* dev_name);
int send_rxgain(const char* dev_name, int gain);
int send_at_command(const char* dev_name, const char* command);
int schedule_restart_event(dev_state_t event, restate_time_t when, const char* dev_name);
int is_valid_phone_number(const char* number);

int str2gain(const char*, int*);
struct ast_str* const gain2str(int);

int str2gain_simcom(const char*, int*);
struct ast_str* const gain2str_simcom(int);

size_t attribute_pure get_esc_str_buffer_size(size_t);
struct ast_str* escape_nstr(const char*, size_t);
struct ast_str* escape_str(const struct ast_str* const);

const char* escape_nstr_ex(struct ast_str*, const char*, size_t);
const char* escape_str_ex(struct ast_str*, const struct ast_str* const);

#define tmp_esc_str(str)                                                         \
    ({                                                                           \
        const size_t STR_TMP_LEN = get_esc_str_buffer_size(ast_str_strlen(str)); \
        struct ast_str* STR_TMP  = ast_str_alloca(STR_TMP_LEN);                  \
        escape_str_ex(STR_TMP, str);                                             \
    })

#define tmp_esc_nstr(str, len)                                   \
    ({                                                           \
        const size_t STR_TMP_LEN = get_esc_str_buffer_size(len); \
        struct ast_str* STR_TMP  = ast_str_alloca(STR_TMP_LEN);  \
        escape_nstr_ex(STR_TMP, str, len);                       \
    })

#endif /* CHAN_QUECTEL_HELPERS_H_INCLUDED */
