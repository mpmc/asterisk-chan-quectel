/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_QUECTEL_CHANNEL_H_INCLUDED
#define CHAN_QUECTEL_CHANNEL_H_INCLUDED

#include "ast_compat.h" /* asterisk compatibility fixes */
#include "ast_config.h"

#include <asterisk/frame.h> /* enum ast_control_frame_type */

typedef struct channel_var {
    const char* name;
    const char* value;
} channel_var_t;

struct pvt;
struct cpvt;

extern struct ast_channel_tech channel_tech;

#if ASTERISK_VERSION_NUM >= 120000 /* 12+ */
struct ast_channel* new_channel(struct pvt* pvt, int ast_state, const char* cid_num, int call_idx, unsigned dir, unsigned state, const char* exten,
                                const struct ast_assigned_ids* assignedids, const struct ast_channel* requestor, unsigned local_channel);
#else  /* 12- */
struct ast_channel* new_channel(struct pvt* pvt, int ast_state, const char* cid_num, int call_idx, unsigned dir, unsigned state, const char* exten,
                                const struct ast_channel* requestor, unsigned local_channel);
#endif /* ^12- */
int queue_control_channel(struct cpvt* cpvt, enum ast_control_frame_type control);
int queue_hangup(struct ast_channel* channel, int hangupcause);
void start_local_channel(struct pvt* pvt, const char* exten, const char* number, const channel_var_t* vars);
void start_local_report_channel(struct pvt* pvt, const char* number, const struct ast_str* const payload, const char* ts, const char* dt, int success,
                                const char report_type, const struct ast_str* const report);
void change_channel_state(struct cpvt* cpvt, unsigned newstate, int cause);
int channels_loop(struct pvt* pvt, const struct ast_channel* requestor);

#endif /* CHAN_QUECTEL_CHANNEL_H_INCLUDED */
