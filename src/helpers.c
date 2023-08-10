/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>
*/
#include <signal.h> /* SIGURG */

#include "ast_config.h"

#include <asterisk/callerid.h> /*  AST_PRES_* */

#include "helpers.h"

#include "at_command.h"
#include "chan_quectel.h" /* devices */
#include "error.h"

// #include "pdu.h"				/* pdu_digit2code() */

static int is_valid_ussd_string(const char* number)
{
    for (; *number; number++) {
        if ((*number >= '0' && *number <= '9') || *number == '*' || *number == '#') {
            continue;
        }
        return 0;
    }
    return 1;
}

#/* */

int is_valid_phone_number(const char* number)
{
    if (number[0] == '+') {
        number++;
    }
    for (; *number; number++) {
        if (*number >= '0' && *number <= '9') {
            continue;
        }
        return 0;
    }
    return 1;
}

#/* */

int get_at_clir_value(struct pvt* pvt, int clir)
{
    int res = 0;

    switch (clir) {
        case AST_PRES_ALLOWED_NETWORK_NUMBER:
        case AST_PRES_ALLOWED_USER_NUMBER_FAILED_SCREEN:
        case AST_PRES_ALLOWED_USER_NUMBER_NOT_SCREENED:
        case AST_PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN:
        case AST_PRES_NUMBER_NOT_AVAILABLE:
            ast_debug(2, "[%s] callingpres: %s\n", PVT_ID(pvt), ast_describe_caller_presentation(clir));
            res = 2;
            break;

        case AST_PRES_PROHIB_NETWORK_NUMBER:
        case AST_PRES_PROHIB_USER_NUMBER_FAILED_SCREEN:
        case AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED:
        case AST_PRES_PROHIB_USER_NUMBER_PASSED_SCREEN:
            ast_debug(2, "[%s] callingpres: %s\n", PVT_ID(pvt), ast_describe_caller_presentation(clir));
            res = 1;
            break;

        default:
            ast_log(LOG_WARNING, "[%s] Unsupported callingpres: %d\n", PVT_ID(pvt), clir);
            if ((clir & AST_PRES_RESTRICTION) != AST_PRES_ALLOWED) {
                res = 0;
            } else {
                res = 2;
            }
            break;
    }

    return res;
}

typedef int (*at_cmd_f)(struct cpvt*, const char*, const char*, unsigned, int, const char*, size_t);

static void free_pvt(struct pvt* pvt) { ast_mutex_unlock(&pvt->lock); }

struct pvt* get_pvt(const char* dev_name, int online)
{
    struct pvt* pvt;
    pvt = find_device_ext(dev_name);
    if (pvt) {
        if (pvt->connected && (!online || (pvt->initialized && pvt->gsm_registered))) {
            return pvt;
        }
        free_pvt(pvt);
    }
    chan_quectel_err = E_DEVICE_DISCONNECTED;
    return NULL;
}

#/* */

int send_ussd(const char* dev_name, const char* ussd)
{
    if (!is_valid_ussd_string(ussd)) {
        chan_quectel_err = E_INVALID_USSD;
        return -1;
    }

    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    int res = at_enqueue_ussd(&pvt->sys_chan, ussd, 0);
    free_pvt(pvt);
    return res;
}

#/* */

int send_sms(const char* const dev_name, const char* const number, const char* const message, int validity, int report, const char* const payload,
             size_t payload_len)
{
    if (!is_valid_phone_number(number)) {
        chan_quectel_err = E_INVALID_PHONE_NUMBER;
        return -1;
    }

    struct pvt* const pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    const int res = at_enqueue_sms(&pvt->sys_chan, number, message, validity, report, payload, payload_len);
    free_pvt(pvt);
    return res;
}

int list_sms(const char* const dev_name, enum msg_status_t stat)
{
    struct pvt* const pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    const int res = at_enqueue_list_messages(&pvt->sys_chan, stat);
    free_pvt(pvt);
    return res;
}

int delete_sms(const char* const dev_name, unsigned int idx, int delflag)
{
    struct pvt* const pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    const int res = at_enqueue_cmgd(&pvt->sys_chan, idx, delflag);
    free_pvt(pvt);
    return res;
}

int sms_direct(const char* const dev_name, int directflag)
{
    struct pvt* const pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }

    if (directflag) {
        const int res = at_enqueue_msg_direct(&pvt->sys_chan, directflag > 0);
        free_pvt(pvt);
        return res;
    }

    if (!CONF_SHARED(pvt, msg_direct)) {
        chan_quectel_err = E_UNKNOWN;
        return -1;
    }

    const int res = at_enqueue_msg_direct(&pvt->sys_chan, CONF_SHARED(pvt, msg_direct) > 0);
    free_pvt(pvt);
    return res;
}

#/* */

int send_reset(const char* dev_name)
{
    struct pvt* pvt = get_pvt(dev_name, 0);
    if (!pvt) {
        return -1;
    }
    int res = at_enqueue_reset(&pvt->sys_chan);
    free_pvt(pvt);
    return res;
}

#/* */

int send_ccwa_set(const char* dev_name, call_waiting_t enable)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    int res = at_enqueue_set_ccwa(&pvt->sys_chan, enable);
    free_pvt(pvt);
    return res;
}

#/* */

int query_qaudloop(const char* dev_name)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    int res = at_enqueue_query_qaudloop(&pvt->sys_chan);
    free_pvt(pvt);
    return res;
}

#/* */

int send_qaudloop(const char* dev_name, int aloop)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    int res = at_enqueue_qaudloop(&pvt->sys_chan, aloop);
    free_pvt(pvt);
    return res;
}

#/* */

int query_qaudmod(const char* dev_name)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    int res = at_enqueue_query_qaudmod(&pvt->sys_chan);
    free_pvt(pvt);
    return res;
}

#/* */

int send_qaudmod(const char* dev_name, int amod)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    int res = at_enqueue_qaudmod(&pvt->sys_chan, amod);
    free_pvt(pvt);


    return res;
}

static const int MAX_GAIN     = 65535;
static const float MAX_GAIN_F = 65535.0f;

static const int MAX_GAIN_SIMCOM     = 8;
static const float MAX_GAIN_SIMCOM_F = 8.0f;

static int to_simcom_gain(int gain) { return gain * MAX_GAIN_SIMCOM / MAX_GAIN; }

#/* */

int query_micgain(const char* dev_name)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    int res;
    if (pvt->is_simcom) {
        res = at_enqueue_query_cmicgain(&pvt->sys_chan);
    } else {
        res = at_enqueue_query_qmic(&pvt->sys_chan);
    }
    free_pvt(pvt);
    return res;
}

#/* */

int send_micgain(const char* dev_name, int gain)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }
    int res;
    if (pvt->is_simcom) {
        res = at_enqueue_cmicgain(&pvt->sys_chan, to_simcom_gain(gain));
    } else {
        res = at_enqueue_qmic(&pvt->sys_chan, gain);
    }
    free_pvt(pvt);
    return res;
}

#/* */

int query_rxgain(const char* dev_name)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }

    int res;
    if (pvt->is_simcom) {
        res = at_enqueue_query_coutgain(&pvt->sys_chan);
    } else {
        res = at_enqueue_query_qrxgain(&pvt->sys_chan);
    }
    free_pvt(pvt);
    return res;
}

#/* */

int send_rxgain(const char* dev_name, int gain)
{
    struct pvt* pvt = get_pvt(dev_name, 1);
    if (!pvt) {
        return -1;
    }

    int res;
    if (pvt->is_simcom) {
        res = at_enqueue_coutgain(&pvt->sys_chan, to_simcom_gain(gain));
    } else {
        res = at_enqueue_qrxgain(&pvt->sys_chan, gain);
    }
    free_pvt(pvt);
    return res;
}

#/* */

int send_at_command(const char* dev_name, const char* command)
{
    struct pvt* pvt = get_pvt(dev_name, 0);
    if (!pvt) {
        return -1;
    }
    int res = at_enqueue_user_cmd(&pvt->sys_chan, command);
    free_pvt(pvt);
    return res;
}

int schedule_restart_event(dev_state_t event, restate_time_t when, const char* dev_name)
{
    struct pvt* pvt = find_device(dev_name);

    if (pvt) {
        pvt->desired_state = event;
        pvt->restart_time  = when;

        pvt_try_restate(pvt);
        ast_mutex_unlock(&pvt->lock);
    } else {
        chan_quectel_err = E_DEVICE_NOT_FOUND;
        return -1;
    }

    return 0;
}

int str2gain(const char* s, int* gain)
{
    if (!s) {
        return -1;
    }

    const size_t len = strlen(s);
    if (!len) {
        return -1;
    }

    if (!strcasecmp(s, "off") || !strcasecmp(s, "mute")) {
        *gain = 0;
        return 0;
    } else if (!strcasecmp(s, "half")) {
        *gain = 32767;
        return 0;
    } else if (!strcasecmp(s, "full")) {
        *gain = 65535;
        return 0;
    }

    if (s[len - 1] == '%') {
        char* const ss        = ast_strndup(s, len - 1);
        const unsigned long p = strtoul(ss, NULL, 10);
        if (errno == ERANGE || p > 100u) {
            ast_free(ss);
            return -1;
        }
        ast_free(ss);
        *gain = (int)(MAX_GAIN_F * p / 100.0f);
        return 0;
    }


    const int g = (int)strtol(s, NULL, 10);
    if (errno == ERANGE || g < 0 || g > MAX_GAIN) {
        return -1;
    }
    *gain = g;
    return 0;
}

struct ast_str* const gain2str(int gain)
{
    struct ast_str* res = ast_str_create(5);
    ast_str_set(&res, 5, "%.0f%%", gain * 100.0f / MAX_GAIN_F);
    return res;
}

int str2gain_simcom(const char* s, int* gain)
{
    if (!s) {
        return -1;
    }

    const size_t len = strlen(s);
    if (!len) {
        return -1;
    }

    if (!strcasecmp(s, "off") || !strcasecmp(s, "mute")) {
        *gain = 0;
        return 0;
    } else if (!strcasecmp(s, "half")) {
        *gain = 4;
        return 0;
    } else if (!strcasecmp(s, "full")) {
        *gain = 8;
        return 0;
    }

    if (s[len - 1] == '%') {
        char* const ss        = ast_strndup(s, len - 1);
        const unsigned long p = strtoul(ss, NULL, 10);
        if (errno == ERANGE || p > 100u) {
            ast_free(ss);
            return -1;
        }
        ast_free(ss);
        *gain = (int)(MAX_GAIN_SIMCOM_F * p / 100.0f);
        return 0;
    }


    const int g = (int)strtol(s, NULL, 10);
    if (errno == ERANGE || g < 0 || g > MAX_GAIN_SIMCOM) {
        return -1;
    }
    *gain = g;
    return 0;
}

struct ast_str* const gain2str_simcom(int gain)
{
    struct ast_str* res = ast_str_create(5);
    ast_str_set(&res, 5, "%.0f%%", gain * 100.0f / MAX_GAIN_SIMCOM_F);
    return res;
}

static char escape_sequences[] = {0x1A, 0x1B, '\a', '\b', '\f', '\n', '\r', '\t', '\v', '\0'};

static char escape_sequences_map[] = {'z', 'e', 'a', 'b', 'f', 'n', 'r', 't', 'v', '\0'};

static char* escape_c(char* dest, const char* s, size_t size)
{
    /*
     * Note - This is an optimized version of ast_escape. When looking only
     * for escape_sequences a couple of checks used in the generic case can
     * be left out thus making it slightly more efficient.
     */
    char* p;
    char* c;

    if (!dest || !size) {
        return dest;
    }
    if (ast_strlen_zero(s)) {
        *dest = '\0';
        return dest;
    }

    for (p = dest; *s && --size; ++s, ++p) {
        /*
         * See if the character to escape is part of the standard escape
         * sequences. If so use its mapped counterpart.
         */
        c = strchr(escape_sequences, *s);
        if (c) {
            if (!--size) {
                /* Not enough room left for the escape sequence. */
                break;
            }

            *p++ = '\\';
            *p   = escape_sequences_map[c - escape_sequences];
        } else {
            *p = *s;
        }
    }
    *p = '\0';

    return dest;
}

size_t attribute_pure get_esc_str_buffer_size(size_t len) { return (len * 2u) + 1u; }

struct ast_str* escape_nstr(const char* buf, size_t cnt)
{
    if (!buf || !cnt) {  // build empty string
        struct ast_str* nbuf = ast_str_create(1);
        ast_str_reset(nbuf);
        return nbuf;
    }

    // build null-terminated string
    struct ast_str* nbuf = ast_str_create(cnt + 1u);
    memcpy(ast_str_buffer(nbuf), buf, cnt);
    *(ast_str_buffer(nbuf) + cnt) = '\000';
    nbuf->used                    = cnt;
    // ast_str_update(nbuf);

    // unescape string
    struct ast_str* const ebuf = ast_str_create(get_esc_str_buffer_size(cnt));
    escape_c(ast_str_buffer(ebuf), ast_str_buffer(nbuf), ast_str_size(ebuf));
    ast_str_update(ebuf);

    ast_free(nbuf);
    return ebuf;
}

const char* escape_nstr_ex(struct ast_str* ebuf, const char* buf, size_t cnt)
{
    if (!cnt) {
        ast_str_reset(ebuf);
        return ast_str_buffer(ebuf);
    }

    // build null-terminated string
    struct ast_str* nbuf = ast_str_create(cnt + 1u);
    memcpy(ast_str_buffer(nbuf), buf, cnt);
    *(ast_str_buffer(nbuf) + cnt) = '\000';
    nbuf->used                    = cnt;
    // ast_str_update(nbuf);

    // unescape string
    const char* const res = escape_str_ex(ebuf, nbuf);
    ast_free(nbuf);
    return res;
}

struct ast_str* escape_str(const struct ast_str* const str)
{
    if (!str || !ast_str_strlen(str)) {
        struct ast_str* nbuf = ast_str_create(1);
        ast_str_reset(nbuf);
        return nbuf;
    }

    const size_t len           = ast_str_strlen(str);
    struct ast_str* const ebuf = ast_str_create(get_esc_str_buffer_size(len));
    escape_c(ast_str_buffer(ebuf), ast_str_buffer(str), ast_str_size(ebuf));
    ast_str_update(ebuf);

    return ebuf;
}

const char* escape_str_ex(struct ast_str* ebuf, const struct ast_str* const str)
{
    const size_t len = ast_str_strlen(str);
    if (ast_str_make_space(&ebuf, get_esc_str_buffer_size(len))) {
        return ast_str_buffer(str);
    }
    escape_c(ast_str_buffer(ebuf), ast_str_buffer(str), ast_str_size(ebuf));
    ast_str_update(ebuf);
    return ast_str_buffer(ebuf);
}
