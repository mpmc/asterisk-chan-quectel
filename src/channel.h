/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_QUECTEL_CHANNEL_H_INCLUDED
#define CHAN_QUECTEL_CHANNEL_H_INCLUDED

#include "ast_config.h"

#include <asterisk/frame.h> /* enum ast_control_frame_type */

typedef struct channel_var {
    const char* name;
    const char* value;
} channel_var_t;

struct pvt;
struct cpvt;

typedef enum local_report_direction { LOCAL_REPORT_DIRECTION_UNKNOWN, LOCAL_REPORT_DIRECTION_INCOMING, LOCAL_REPORT_DIRECTION_OUTGOING } local_report_direction;

extern struct ast_channel_tech channel_tech;

struct ast_channel* new_channel(struct pvt* pvt, int ast_state, const char* cid_num, int call_idx, unsigned dir, unsigned state, const char* exten,
                                const struct ast_assigned_ids* assignedids, const struct ast_channel* requestor, unsigned local_channel);

int channels_loop(struct pvt* pvt, const struct ast_channel* requestor);

int queue_hangup(struct ast_channel* channel, int hangupcause);

void start_local_channel(struct pvt* pvt, const char* exten, const char* number, const channel_var_t* vars);
void start_local_report_channel(struct pvt* pvt, const char* subject, local_report_direction direction, const char* number, const char* ts, const char* dt,
                                int success, struct ast_json* const report);

#endif /* CHAN_QUECTEL_CHANNEL_H_INCLUDED */
