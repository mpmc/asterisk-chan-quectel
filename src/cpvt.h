/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_QUECTEL_CPVT_H_INCLUDED
#define CHAN_QUECTEL_CPVT_H_INCLUDED

#include <ptime-config.h>

#include "ast_config.h"

#include <asterisk/frame.h>       /* AST_FRIENDLY_OFFSET */
#include <asterisk/linkedlists.h> /* AST_LIST_ENTRY() */

#include "mixbuffer.h" /* struct mixstream */
#include "mutils.h"    /* enum2str() ITEMS_OF() */

typedef enum {
    CALL_STATE_MIN = 0,

    /* values from CLCC */
    CALL_STATE_ACTIVE = CALL_STATE_MIN, /*!< comes from CLCC */
    CALL_STATE_ONHOLD,                  /*!< comes from CLCC */
    CALL_STATE_DIALING,                 /*!< comes from CLCC */
    CALL_STATE_ALERTING,                /*!< comes from CLCC */
    CALL_STATE_INCOMING,                /*!< comes from CLCC */
    CALL_STATE_WAITING,                 /*!< comes from CLCC */

    CALL_STATE_RELEASED, /*!< on CEND or channel_hangup() called */
    CALL_STATE_INIT,     /*!< channel_call() called */
    CALL_STATE_MAX = CALL_STATE_INIT
} call_state_t;

#define CALL_STATES_NUMBER (CALL_STATE_MAX - CALL_STATE_MIN + 1)

typedef enum {
    CALL_FLAG_NONE          = 0,
    CALL_FLAG_HOLD_OTHER    = 1,   /*!< external, from channel_call() hold other calls and dial this number */
    CALL_FLAG_NEED_HANGUP   = 2,   /*!< internal, require issue AT+CHUP or AT+CHLD=1x for call */
    CALL_FLAG_ACTIVATED     = 4,   /*!< internal, fd attached to channel fds list */
    CALL_FLAG_ALIVE         = 8,   /*!< internal, temporary, still listed in CLCC */
    CALL_FLAG_CONFERENCE    = 16,  /*!< external, from dial() begin conference after activate this call */
    CALL_FLAG_MASTER        = 32,  /*!< internal, channel fd[0] is pvt->audio_fd and  fd[1] is timer fd */
    CALL_FLAG_BRIDGE_LOOP   = 64,  /*!< internal, found channel bridged to channel on same device */
    CALL_FLAG_BRIDGE_CHECK  = 128, /*!< internal, we already do check for bridge loop */
    CALL_FLAG_MULTIPARTY    = 256, /*!< internal, CLCC mpty is 1 */
    CALL_FLAG_DIRECTION     = 512, /*!< call direction */
    CALL_FLAG_LOCAL_CHANNEL = 1024 /*!< local channel flag */
} call_flag_t;

#define CALL_DIR_INCOMING 1u
#define CALL_DIR_OUTGOING 0u

typedef struct cpvt {
    AST_LIST_ENTRY(cpvt) entry; /*!< linked list pointers */

    struct ast_channel* channel; /*!< Channel pointer */
    struct pvt* pvt;             /*!< pointer to device structure */

    short call_idx; /*!< device call ID */
#define MIN_CALL_IDX 0
#define MAX_CALL_IDX 31

    call_state_t state; /*!< see also call_state_t */
    unsigned int flags; /*!< see also call_flag_t */

    int rd_pipe[2]; /*!< pipe for split read from device */
#define PIPE_READ 0
#define PIPE_WRITE 1

    struct mixstream mixstream; /*!< mix stream */

    void* read_buf;              /*!< audio read buffer */
    struct ast_frame read_frame; /*!< voice frame */
} cpvt_t;

#define CPVT_SET_FLAG(cpvt, flag) ast_set2_flag(cpvt, 1, flag)
#define CPVT_SET_FLAGS(cpvt, flag) ast_set_flag(cpvt, flag)
#define CPVT_RESET_FLAG(cpvt, flag) ast_set2_flag(cpvt, 0, flag)
#define CPVT_RESET_FLAGS(cpvt, flag) ast_clear_flag(cpvt, flag)
#define CPVT_TEST_FLAG(cpvt, flag) ast_test_flag(cpvt, flag)

#define CPVT_IS_MASTER(cpvt) CPVT_TEST_FLAG(cpvt, CALL_FLAG_MASTER)

#define CPVT_DIR_INCOMING(cpvt) CPVT_TEST_FLAG(cpvt, CALL_FLAG_DIRECTION)
#define CPVT_DIR_OUTGOING(cpvt) (!CPVT_TEST_FLAG(cpvt, CALL_FLAG_DIRECTION))
#define CPVT_DIRECTION(cpvt) (CPVT_TEST_FLAG(cpvt, CALL_FLAG_DIRECTION) ? CALL_DIR_INCOMING : CALL_DIR_OUTGOING)
#define CPVT_SET_DIRECTION(cpvt, dir) ast_set2_flag(cpvt, dir, CALL_FLAG_DIRECTION)

#define CPVT_IS_LOCAL(cpvt) CPVT_TEST_FLAG(cpvt, CALL_FLAG_LOCAL_CHANNEL)
#define CPVT_SET_LOCAL(cpvt, local) ast_set2_flag(cpvt, local, CALL_FLAG_LOCAL_CHANNEL)

// state
#define CPVT_IS_ACTIVE(cpvt) ((cpvt)->state == CALL_STATE_ACTIVE)
#define CPVT_IS_SOUND_SOURCE(cpvt) ((cpvt)->state <= CALL_STATE_INCOMING || (cpvt)->state == CALL_STATE_INIT)

struct cpvt* cpvt_alloc(struct pvt* pvt, int call_idx, unsigned dir, call_state_t statem, unsigned local_channel);
void cpvt_free(struct cpvt* cpvt);

struct cpvt* pvt_find_cpvt(struct pvt* pvt, int call_idx);
struct cpvt* active_cpvt(struct pvt* pvt);
struct cpvt* last_initialized_cpvt(struct pvt* pvt);
const char* pvt_call_dir(const struct pvt* pvt);

#/* */

static inline const char* call_state2str(call_state_t state)
{
    static const char* const states[] = {/* real device states */
                                         "active", "held", "dialing", "alerting", "incoming", "waiting",

                                         /* pseudo states */
                                         "released", "init"};

    return enum2str(state, states, ITEMS_OF(states));
}

void lock_cpvt(struct cpvt* const);
void try_lock_cpvt(struct cpvt* const);
void unlock_cpvt(struct cpvt* const);

#define SCOPED_CPVT(varname, lock) SCOPED_LOCK(varname, lock, lock_cpvt, unlock_cpvt)
#define SCOPED_CPVT_TL(varname, lock) SCOPED_LOCK(varname, lock, try_lock_cpvt, unlock_cpvt)

#endif /* CHAN_QUECTEL_CPVT_H_INCLUDED */
