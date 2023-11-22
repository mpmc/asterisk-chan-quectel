/*
    monitor_thread.c
*/

#include <signal.h>  /* SIGURG */
#include <termios.h> /* struct termios tcgetattr() tcsetattr()  */

#include "ast_config.h"

#include <asterisk/lock.h>
#include <asterisk/strings.h>
#include <asterisk/taskprocessor.h>
#include <asterisk/threadpool.h>

#include "monitor_thread.h"

#include "at_queue.h"
#include "at_read.h"
#include "chan_quectel.h"
#include "channel.h"
#include "helpers.h"
#include "smsdb.h"

static struct ast_threadpool* threadpool_create(const char* const dev)
{
    static const size_t TP_DEF_LEN = 32;
    static const size_t TP_MAX_LEN = 512;

    static const struct ast_threadpool_options options = {
        .version = AST_THREADPOOL_OPTIONS_VERSION, .idle_timeout = 0, .auto_increment = 0, .initial_size = 1, .max_size = 1};

    RAII_VAR(struct ast_str*, threadpool_name, ast_str_create(TP_DEF_LEN), ast_free);
    ast_str_set(&threadpool_name, TP_MAX_LEN, "chan-quectel/%s", dev);
    return ast_threadpool_create(ast_str_buffer(threadpool_name), NULL, &options);
}

static struct ast_taskprocessor* threadpool_serializer(struct ast_threadpool* pool, const char* const dev)
{
    char taskprocessor_name[AST_TASKPROCESSOR_MAX_NAME + 1];
    ast_taskprocessor_build_name(taskprocessor_name, sizeof(taskprocessor_name), "chan-quectel/%s", dev);
    return ast_threadpool_serializer(taskprocessor_name, pool);
}

static void handle_expired_reports(struct pvt* pvt)
{
    static const size_t SMSDB_DEF_LEN = 64;

    RAII_VAR(struct ast_str*, dst, ast_str_create(SMSDB_DEF_LEN), ast_free);
    RAII_VAR(struct ast_str*, msg, ast_str_create(SMSDB_DEF_LEN), ast_free);

    int uid;

    const ssize_t res = smsdb_outgoing_purge_one(&uid, &dst, &msg);
    if (res < 0) {
        return;
    }

    ast_verb(3, "[%s][SMS:%d %s] Expired\n", PVT_ID(pvt), uid, ast_str_buffer(dst));

    RAII_VAR(struct ast_json*, report, ast_json_object_create(), ast_json_unref);
    ast_json_object_set(report, "info", ast_json_string_create("Message expired"));
    ast_json_object_set(report, "uid", ast_json_integer_create(uid));
    ast_json_object_set(report, "expired", ast_json_integer_create(1));
    AST_JSON_OBJECT_SET(report, msg);
    start_local_report_channel(pvt, "sms", LOCAL_REPORT_DIRECTION_OUTGOING, ast_str_buffer(dst), NULL, NULL, 0, report);
}

static int handle_expired_reports_taskproc(void* tpdata) { return pvt_taskproc_trylock_and_execute(tpdata, handle_expired_reports); }

static void cmd_timeout(struct pvt* const pvt)
{
    if (!pvt->terminate_monitor) {
        return;
    }

    const struct at_queue_cmd* const ecmd = at_queue_head_cmd(pvt);
    if (!ecmd || ecmd->length) {
        return;
    }

    if (at_response(pvt, &pvt->empty_str, RES_TIMEOUT)) {
        ast_log(LOG_ERROR, "[%s] Fail to handle response\n", PVT_ID(pvt));
        pvt->terminate_monitor = 1;
        return;
    }

    if (ecmd->flags & ATQ_CMD_FLAG_IGNORE) {
        return;
    }

    pvt->terminate_monitor = 1;
}

static int cmd_timeout_taskproc(void* tpdata) { return pvt_taskproc_trylock_and_execute(tpdata, cmd_timeout); }

/*!
 * Get status of the quectel. It might happen that the device disappears
 * (e.g. due to a USB unplug).
 *
 * \return 0 if device seems ok, non-0 if it seems not available
 */

static int port_status(int fd, int* err)
{
    struct termios t;

    if (fd < 0) {
        if (err) {
            *err = EINVAL;
        }
        return -1;
    }

    const int res = tcgetattr(fd, &t);
    if (res) {
        if (err) {
            *err = errno;
        }
    }
    return res;
}

static int is_snd_pcm_disconnected(snd_pcm_t* const pcm)
{
    const snd_pcm_state_t state = snd_pcm_state(pcm);
    return (state == SND_PCM_STATE_DISCONNECTED);
}

static int alsa_status(const char* const pvt_id, snd_pcm_t* const pcm_playback, snd_pcm_t* const pcm_capture)
{
    if (is_snd_pcm_disconnected(pcm_playback) || is_snd_pcm_disconnected(pcm_capture)) {
        return -1;
    }

    return 0;
}

static int reopen_audio_port(struct pvt* pvt)
{
    closetty_lck(PVT_STATE(pvt, audio_tty), pvt->audio_fd, 0, 0);
    pvt->audio_fd = opentty(PVT_STATE(pvt, audio_tty), pvt->is_simcom);

    if (!PVT_NO_CHANS(pvt)) {
        struct cpvt* cpvt;
        AST_LIST_TRAVERSE(&(pvt->chans), cpvt, entry) {
            ast_channel_set_fd(cpvt->channel, 0, pvt->audio_fd);
        }
    }

    return (pvt->audio_fd > 0);
}

static int check_dev_status(struct pvt* const pvt)
{
    int err;
    if (port_status(pvt->data_fd, &err)) {
        ast_log(LOG_ERROR, "[%s][DATA] Lost connection: %s\n", PVT_ID(pvt), strerror(err));
        return -1;
    }

    switch (CONF_UNIQ(pvt, uac)) {
        case TRIBOOL_FALSE:
            if (port_status(pvt->audio_fd, &err)) {
                if (reopen_audio_port(pvt)) {
                    ast_log(LOG_WARNING, "[%s][AUDIO][TTY] Lost connection: %s\n", PVT_ID(pvt), strerror(err));
                } else {
                    ast_log(LOG_ERROR, "[%s][AUDIO][TTY] Lost connection: %s\n", PVT_ID(pvt), strerror(err));
                    return -1;
                }
            }
            break;

        case TRIBOOL_TRUE:
            show_alsa_state(2, "PLAYBACK", PVT_ID(pvt), pvt->ocard);
            show_alsa_state(2, "CAPTURE", PVT_ID(pvt), pvt->icard);
            break;

        case TRIBOOL_NONE:
            show_alsa_state(2, "PLAYBACK", PVT_ID(pvt), pvt->ocard);
            show_alsa_state(2, "CAPTURE", PVT_ID(pvt), pvt->icard);
            if (alsa_status(PVT_ID(pvt), pvt->ocard, pvt->icard)) {
                ast_log(LOG_ERROR, "[%s][AUDIO][ALSA] Lost connection\n", PVT_ID(pvt));
                return -1;
            }
            break;
    }
    return 0;
}

static void monitor_threadproc_pvt(struct pvt* const pvt)
{
    static const size_t RINGBUFFER_SIZE = 2 * 1024;
    static const int DATA_READ_TIMEOUT  = 10000;

    struct ringbuffer rb;
    RAII_VAR(void* const, buf, ast_calloc(1, RINGBUFFER_SIZE), ast_free);
    rb_init(&rb, buf, RINGBUFFER_SIZE);

    RAII_VAR(struct ast_str* const, result, ast_str_create(RINGBUFFER_SIZE), ast_free);

    ast_mutex_lock(&pvt->lock);
    RAII_VAR(char* const, dev, ast_strdup(PVT_ID(pvt)), ast_free);

    RAII_VAR(struct ast_threadpool*, threadpool, threadpool_create(dev), ast_threadpool_shutdown);
    if (!threadpool) {
        ast_log(LOG_ERROR, "[%s] Error initializing threadpool\n", dev);
        goto e_cleanup;
    }

    RAII_VAR(struct ast_taskprocessor*, tps, threadpool_serializer(threadpool, dev), ast_taskprocessor_unreference);
    if (!tps) {
        ast_log(LOG_ERROR, "[%s] Error initializing taskprocessor\n", dev);
        goto e_cleanup;
    }

    /* 4 reduce locking time make copy of this readonly fields */
    const int fd = pvt->data_fd;
    clean_read_data(dev, fd, &rb);

    /* schedule initilization  */
    if (at_enqueue_initialization(&pvt->sys_chan)) {
        ast_log(LOG_ERROR, "[%s] Error adding initialization commands to queue\n", dev);
        goto e_cleanup;
    }

    ast_mutex_unlock(&pvt->lock);

    int read_result = 0;
    while (1) {
        if (ast_taskprocessor_push(tps, handle_expired_reports_taskproc, pvt)) {
            ast_debug(5, "[%s] Unable to handle exprired reports\n", dev);
        }

        if (ast_mutex_trylock(&pvt->lock)) {  // pvt unlocked
            int t = DATA_READ_TIMEOUT;
            if (!at_wait(fd, &t)) {
                if (ast_taskprocessor_push(tps, at_enqueue_ping_taskproc, pvt)) {
                    ast_debug(5, "[%s] Unable to handle timeout\n", dev);
                }
                continue;
            }
        } else {  // pvt locked
            if (check_dev_status(pvt)) {
                goto e_cleanup;
            }

            if (pvt->terminate_monitor) {
                ast_log(LOG_NOTICE, "[%s] Stopping by %s request\n", dev, dev_state2str(pvt->desired_state));
                goto e_restart;
            }

            int t;
            int is_cmd_timeout = 1;
            if (at_queue_timeout(pvt, &t)) {
                is_cmd_timeout = 0;
            }

            ast_mutex_unlock(&pvt->lock);

            if (is_cmd_timeout) {
                if (t < 0 || !at_wait(fd, &t)) {
                    if (ast_taskprocessor_push(tps, cmd_timeout_taskproc, pvt)) {
                        ast_debug(5, "[%s] Unable to handle timeout\n", dev);
                    }
                    continue;
                }
            } else {
                t = DATA_READ_TIMEOUT;
                if (!at_wait(fd, &t)) {
                    if (ast_taskprocessor_push(tps, at_enqueue_ping_taskproc, pvt)) {
                        ast_debug(5, "[%s] Unable to handle timeout\n", dev);
                    }
                    continue;
                }
            }
        }

        /* FIXME: access to device not locked */
        int iovcnt = at_read(fd, dev, &rb);
        if (iovcnt < 0) {
            break;
        }

        if (!ast_mutex_trylock(&pvt->lock)) {
            PVT_STAT(pvt, d_read_bytes) += iovcnt;
            ast_mutex_unlock(&pvt->lock);
        }

        struct iovec iov[2];
        size_t skip = 0u;

        while ((iovcnt = at_read_result_iov(dev, &read_result, &skip, &rb, iov, result)) > 0) {
            const size_t len = at_combine_iov(result, iov, iovcnt);
            rb_read_upd(&rb, len + skip);
            skip = 0u;
            if (!len) {
                continue;
            }

            at_response_taskproc_data* const tpdata = at_response_taskproc_data_alloc(pvt, result);
            if (tpdata) {
                if (ast_taskprocessor_push(tps, at_response_taskproc, tpdata)) {
                    ast_log(LOG_ERROR, "[%s] Fail to handle response\n", dev);
                    ast_free(tpdata);
                    goto e_restart;
                }
            }
        }
    }

    ast_mutex_lock(&pvt->lock);

e_cleanup:
    if (!pvt->initialized) {
        // TODO: send monitor event
        ast_verb(3, "[%s] Error initializing channel\n", dev);
    }
    /* it real, unsolicited disconnect */
    pvt->terminate_monitor = 0;

e_restart:
    pvt_disconnect(pvt);
    //	pvt->monitor_running = 0;
    ast_mutex_unlock(&pvt->lock);
}

static void* monitor_threadproc(void* _pvt)
{
    struct pvt* const pvt = _pvt;
    monitor_threadproc_pvt(pvt);
    /* TODO: wakeup discovery thread after some delay */
    return NULL;
}

int pvt_monitor_start(struct pvt* pvt)
{
    if (ast_pthread_create_background(&pvt->monitor_thread, NULL, monitor_threadproc, pvt) < 0) {
        pvt->monitor_thread = AST_PTHREADT_NULL;
        return 0;
    }

    return 1;
}

void pvt_monitor_stop(struct pvt* pvt)
{
    if (pvt->monitor_thread == AST_PTHREADT_NULL) {
        return;
    }

    pvt->terminate_monitor = 1;
    pthread_kill(pvt->monitor_thread, SIGURG);

    {
        const pthread_t id = pvt->monitor_thread;
        SCOPED_LOCK(pvt_lock, &pvt->lock, ast_mutex_unlock, ast_mutex_lock);  // scoped UNlock
        pthread_join(id, NULL);
    }

    pvt->terminate_monitor = 0;
    pvt->monitor_thread    = AST_PTHREADT_NULL;
}
