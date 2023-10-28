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
    CALL_FLAG_NONE         = 0,
    CALL_FLAG_HOLD_OTHER   = 1,   /*!< external, from channel_call() hold other calls and dial this number */
    CALL_FLAG_NEED_HANGUP  = 2,   /*!< internal, require issue AT+CHUP or AT+CHLD=1x for call */
    CALL_FLAG_ACTIVATED    = 4,   /*!< internal, fd attached to channel fds list */
    CALL_FLAG_ALIVE        = 8,   /*!< internal, temporary, still listed in CLCC */
    CALL_FLAG_CONFERENCE   = 16,  /*!< external, from dial() begin conference after activate this call */
    CALL_FLAG_MASTER       = 32,  /*!< internal, channel fd[0] is pvt->audio_fd and  fd[1] is timer fd */
    CALL_FLAG_BRIDGE_LOOP  = 64,  /*!< internal, found channel bridged to channel on same device */
    CALL_FLAG_BRIDGE_CHECK = 128, /*!< internal, we already do check for bridge loop */
    CALL_FLAG_MULTIPARTY   = 256, /*!< internal, CLCC mpty is 1 */
} call_flag_t;

/* */
typedef struct cpvt {
    AST_LIST_ENTRY(cpvt) entry; /*!< linked list pointers */

    struct ast_channel* channel; /*!< Channel pointer */
    struct pvt* pvt;             /*!< pointer to device structure */

    short call_idx; /*!< device call ID */
#define MIN_CALL_IDX 0
#define MAX_CALL_IDX 31

    call_state_t state; /*!< see also call_state_t */
    int flags;          /*!< see also call_flag_t */

    /* TODO: join with flags */
    unsigned int dir          :1; /*!< call direction */
    unsigned int local_channel:1; /*!< local channel flag */
#define CALL_DIR_OUTGOING 0
#define CALL_DIR_INCOMING 1

    int rd_pipe[2]; /*!< pipe for split read from device */
#define PIPE_READ 0
#define PIPE_WRITE 1

    struct mixstream mixstream; /*!< mix stream */

    void* read_buf;              /*!< audio read buffer */
    struct ast_frame read_frame; /*!< voice frame */
} cpvt_t;

#define CPVT_SET_FLAGS(cpvt, flag) \
    do {                           \
        (cpvt)->flags |= (flag);   \
    } while (0)

#define CPVT_RESET_FLAGS(cpvt, flag)   \
    do {                               \
        (cpvt)->flags &= ~((int)flag); \
    } while (0)

#define CPVT_TEST_FLAG(cpvt, flag) ((cpvt)->flags & (flag))
#define CPVT_TEST_FLAGS(cpvt, flag) (((cpvt)->flags & (flag)) == (flag))

#define CPVT_IS_MASTER(cpvt) CPVT_TEST_FLAG(cpvt, CALL_FLAG_MASTER)
#define CPVT_IS_ACTIVE(cpvt) ((cpvt)->state == CALL_STATE_ACTIVE)
#define CPVT_IS_SOUND_SOURCE(cpvt) ((cpvt)->state == CALL_STATE_ACTIVE || (cpvt)->state == CALL_STATE_DIALING || (cpvt)->state == CALL_STATE_ALERTING)

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
                                         "released", "initialize"};

    return enum2str(state, states, ITEMS_OF(states));
}

void lock_cpvt(struct cpvt* const);
void try_lock_cpvt(struct cpvt* const);
void unlock_cpvt(struct cpvt* const);

#define SCOPED_CPVT(varname, lock) SCOPED_LOCK(varname, lock, lock_cpvt, unlock_cpvt)
#define SCOPED_CPVT_TL(varname, lock) SCOPED_LOCK(varname, lock, try_lock_cpvt, unlock_cpvt)

#endif /* CHAN_QUECTEL_CPVT_H_INCLUDED */
