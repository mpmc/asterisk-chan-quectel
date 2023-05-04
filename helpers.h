/*
   Copyright (C) 2010,2011 bg <bg_one@mail.ru>
*/
#ifndef CHAN_QUECTEL_HELPERS_H_INCLUDED
#define CHAN_QUECTEL_HELPERS_H_INCLUDED

#include "dc_config.h"			/* call_waiting_t */
#include "chan_quectel.h"		/* restate_time_t */
#include <asterisk/strings.h>

int get_at_clir_value (struct pvt* pvt, int clir);

/* return status string of sending, status arg is optional */
int send_ussd(const char *dev_name, const char *ussd);
int send_sms(const char *dev_name, const char *number, const char *message, const char *validity, const char *report, const char *payload, size_t payload_len);
int send_reset(const char *dev_name);
int send_ccwa_set(const char *dev_name, call_waiting_t enable);
int send_at_command(const char *dev_name, const char *command);
int schedule_restart_event(dev_state_t event, restate_time_t when, const char *dev_name);
int is_valid_phone_number(const char * number);

struct ast_str* escape_nstr(const char*, size_t);
struct ast_str* escape_str(const struct ast_str* const);

#endif /* CHAN_QUECTEL_HELPERS_H_INCLUDED */
