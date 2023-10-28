/*
   Copyright (C) 2010,2011 bg <bg_one@mail.ru>
*/
#include <fcntl.h>
#include <unistd.h>

#include "ast_config.h"

#include <asterisk/utils.h>

#include "cpvt.h"

#include "at_queue.h"     /* struct at_queue_task */
#include "chan_quectel.h" /* struct pvt */
#include "mutils.h"       /* ITEMS_OF() */

#/* return 0 on success */

// TODO: move to activation time, save resources
static int init_pipe(int filedes[2])
{
    int rv = pipe(filedes);
    if (!rv) {
        for (int x = 0; x < 2; ++x) {
            rv              = fcntl(filedes[x], F_GETFL);
            const int flags = fcntl(filedes[x], F_GETFD);
            if (rv == -1 || flags == -1 || (rv = fcntl(filedes[x], F_SETFL, O_NONBLOCK | rv)) == -1 ||
                (rv = fcntl(filedes[x], F_SETFD, flags | FD_CLOEXEC)) == -1) {
                goto bad;
            }
        }
        return 0;
bad:
        close(filedes[0]);
        close(filedes[1]);
    }
    return rv;
}

#/* */

struct cpvt* cpvt_alloc(struct pvt* pvt, int call_idx, unsigned dir, call_state_t state, unsigned local_channel)
{
    int fd[2] = {-1, -1};

    if (CONF_SHARED(pvt, multiparty)) {
        if (init_pipe(fd)) {
            return NULL;
        }
    }

    struct cpvt* const cpvt = ast_calloc(1, sizeof(*cpvt));
    if (!cpvt) {
        if (CONF_SHARED(pvt, multiparty)) {
            close(fd[0]);
            close(fd[1]);
        }
        return NULL;
    }

    const struct ast_format* const fmt = pvt_get_audio_format(pvt);
    const size_t buffer_size           = pvt_get_audio_frame_size(PTIME_PLAYBACK, fmt);

    cpvt->pvt           = pvt;
    cpvt->call_idx      = call_idx;
    cpvt->state         = state;
    cpvt->dir           = dir;
    cpvt->local_channel = local_channel;
    cpvt->rd_pipe[0]    = fd[0];
    cpvt->rd_pipe[1]    = fd[1];
    cpvt->read_buf      = ast_calloc(1, buffer_size + AST_FRIENDLY_OFFSET);

    AST_LIST_INSERT_TAIL(&pvt->chans, cpvt, entry);
    if (PVT_NO_CHANS(pvt)) {
        pvt_on_create_1st_channel(pvt);
    }
    PVT_STATE(pvt, chansno)++;
    PVT_STATE(pvt, chan_count[cpvt->state])++;

    ast_debug(3, "[%s] Create cpvt - idx:%d dir:%d state:%s buffer_len:%u\n", PVT_ID(pvt), call_idx, dir, call_state2str(state), (unsigned int)buffer_size);
    return cpvt;
}

static void decrease_chan_counters(const struct cpvt* const cpvt, struct pvt* const pvt)
{
    struct cpvt* found;

    AST_LIST_TRAVERSE_SAFE_BEGIN(&pvt->chans, found, entry)
        if (found == cpvt) {
            AST_LIST_REMOVE_CURRENT(entry);
            PVT_STATE(pvt, chan_count[cpvt->state])--;
            PVT_STATE(pvt, chansno)--;
            break;
        }
    AST_LIST_TRAVERSE_SAFE_END;
}

static void relink_to_sys_chan(const struct cpvt* const cpvt, struct pvt* const pvt)
{
    struct at_queue_task* task;

    /* relink task to sys_chan */
    AST_LIST_TRAVERSE(&pvt->at_queue, task, entry) {
        if (task->cpvt == cpvt) {
            task->cpvt = &pvt->sys_chan;
        }
    }
}

void cpvt_free(struct cpvt* cpvt)
{
    struct pvt* const pvt = cpvt->pvt;

    ast_debug(3, "[%s] Destroy cpvt - idx:%d dir:%d state:%s flags:%d channel:%s\n", PVT_ID(pvt), cpvt->call_idx, cpvt->dir, call_state2str(cpvt->state),
              cpvt->flags, cpvt->channel ? "attached" : "detached");

    if (PVT_NO_CHANS(pvt)) {
        pvt_on_remove_last_channel(pvt);
        pvt_try_restate(pvt);
    }

    decrease_chan_counters(cpvt, pvt);
    relink_to_sys_chan(cpvt, pvt);

    ast_free(cpvt->read_buf);
    cpvt->read_buf = NULL;

    close(cpvt->rd_pipe[1]);
    close(cpvt->rd_pipe[0]);

    ast_free(cpvt);
}

#/* */

struct cpvt* pvt_find_cpvt(struct pvt* pvt, int call_idx)
{
    struct cpvt* cpvt;

    AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
        if (call_idx == cpvt->call_idx) {
            return cpvt;
        }
    }

    return 0;
}

struct cpvt* active_cpvt(struct pvt* pvt)
{
    struct cpvt* cpvt;

    AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
        if (CPVT_IS_SOUND_SOURCE(cpvt)) {
            return cpvt;
        }
    }

    return 0;
}

struct cpvt* last_initialized_cpvt(struct pvt* pvt)
{
    struct cpvt* cpvt;
    struct cpvt* res = NULL;

    AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
        if (CPVT_IS_SOUND_SOURCE(cpvt) || (cpvt)->state == CALL_STATE_INIT) {
            res = cpvt;
        }
    }

    return res;
}

#/* */

const char* pvt_call_dir(const struct pvt* pvt)
{
    static const char* dirs[] = {"Active", "Outgoing", "Incoming", "Both"};

    int index = 0;
    struct cpvt* cpvt;

    AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
        if (cpvt->dir == CALL_DIR_OUTGOING) {
            index |= 0x1;
        } else {
            index |= 0x2;
        }
    }

    return dirs[index];
}

void lock_cpvt(struct cpvt* const cpvt)
{
    struct pvt* const pvt = cpvt->pvt;
    if (!pvt) {
        return;
    }

    ast_mutex_trylock(&pvt->lock);
}

void try_lock_cpvt(struct cpvt* const cpvt)
{
    struct pvt* const pvt = cpvt->pvt;
    if (!pvt) {
        return;
    }

    struct ast_channel* const channel = cpvt->channel;
    if (!channel) {
        return;
    }

    ast_mutex_t* const mutex = &pvt->lock;

    while (ast_mutex_trylock(mutex)) {
        CHANNEL_DEADLOCK_AVOIDANCE(channel);
    }
}

void unlock_cpvt(struct cpvt* const cpvt)
{
    if (!cpvt) {
        return;
    }
    unlock_pvt(cpvt->pvt);
}
