/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>
*/

/*
   Copyright (C) 2009 - 2010 Artem Makhutov
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/

#include "ast_config.h"

#include <asterisk/causes.h>
#include <asterisk/utils.h>

#include "at_command.h"

#include "at_queue.h"
#include "chan_quectel.h" /* struct pvt */
#include "char_conv.h"    /* char_to_hexstr_7bit() */
#include "error.h"
#include "pdu.h" /* build_pdu() */
#include "smsdb.h"

static const char cmd_at[]    = "AT\r";
static const char cmd_chld2[] = "AT+CHLD=2\r";

/*!
 * \brief Get the string representation of the given AT command
 * \param cmd -- the command to process
 * \return a string describing the given command
 */

const char* at_cmd2str(at_cmd_t cmd)
{
    static const char* const cmds[] = {AT_COMMANDS_TABLE(AT_CMD_AS_STRING)};

    return enum2str_def(cmd, cmds, ITEMS_OF(cmds), "UNDEFINED");
}

/*!
 * \brief Format and fill generic command
 * \param cmd -- the command structure
 * \param format -- printf format string
 * \param ap -- list of arguments
 * \return 0 on success
 */

static int __attribute__((format(printf, 2, 0))) at_fill_generic_cmd_va(at_queue_cmd_t* cmd, const char* format, va_list ap)
{
    static const ssize_t AT_CMD_LEN     = 32;
    static const ssize_t MAX_AT_CMD_LEN = 4096;

    struct ast_str* cmdstr = ast_str_create(AT_CMD_LEN);
    ast_str_set_va(&cmdstr, MAX_AT_CMD_LEN, format, ap);
    const size_t cmdlen = ast_str_strlen(cmdstr);

    if (!cmdlen) {
        ast_free(cmdstr);
        return -1;
    }

    cmd->data   = ast_strndup(ast_str_buffer(cmdstr), cmdlen);
    cmd->length = (unsigned int)cmdlen;
    cmd->flags &= ~ATQ_CMD_FLAG_STATIC;
    ast_free(cmdstr);
    return 0;
}

/*!
 * \brief Format and fill generic command
 * \param cmd -- the command structure
 * \param format -- printf format string
 * \return 0 on success
 */

static int __attribute__((format(printf, 2, 3))) at_fill_generic_cmd(at_queue_cmd_t* cmd, const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    const int rv = at_fill_generic_cmd_va(cmd, format, ap);
    va_end(ap);

    return rv;
}

/*!
 * \brief Enqueue generic command
 * \param pvt -- pvt structure
 * \param cmd -- at command
 * \param prio -- queue priority of this command
 * \param format -- printf format string including AT command text
 * \return 0 on success
 */

static int __attribute__((format(printf, 4, 5))) at_enqueue_generic(struct cpvt* cpvt, at_cmd_t cmd, int prio, const char* format, ...)
{
    va_list ap;

    at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_DYN(cmd);

    va_start(ap, format);
    const int rv = at_fill_generic_cmd_va(&at_cmd, format, ap);
    va_end(ap);

    if (rv) {
        return rv;
    }

    return at_queue_insert(cpvt, &at_cmd, 1, prio);
}

int at_enqueue_at(struct cpvt* cpvt)
{
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_STFT(CMD_AT, RES_OK, cmd_at, 0, ATQ_CMD_TIMEOUT_SHORT, 0);

    if (at_queue_insert_const(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue initialization commands
 * \param cpvt -- cpvt structure
 * \param from_command -- begin initialization from this command in list
 * \return 0 on success
 */
int at_enqueue_initialization(struct cpvt* cpvt)
{
    static const char cmd_at[]   = "AT\r";
    static const char cmd_z[]    = "ATZ\r";
    static const char cmd_ate0[] = "ATE0\r";

    static const char cmd_cgmi[] = "AT+CGMI\r";
    static const char cmd_csca[] = "AT+CSCA?\r";
    static const char cmd_cgmm[] = "AT+CGMM\r";
    static const char cmd_cgmr[] = "AT+CGMR\r";

    static const char cmd_cmee[] = "AT+CMEE=0\r";
    static const char cmd_cgsn[] = "AT+CGSN\r";
    static const char cmd_cimi[] = "AT+CIMI\r";
    static const char cmd_cpin[] = "AT+CPIN?\r";

    static const char cmd_cops[]   = "AT+COPS=3,0\r";
    static const char cmd_creg_2[] = "AT+CREG=2\r";
    static const char cmd_creg[]   = "AT+CREG?\r";
    static const char cmd_cnum[]   = "AT+CNUM\r";

    static const char cmd_cssn[] = "AT+CSSN=1,1\r";
    static const char cmd_cmgf[] = "AT+CMGF=0\r";
    static const char cmd_cscs[] = "AT+CSCS=\"UCS2\"\r";

    static const at_queue_cmd_t st_cmds[] = {
        ATQ_CMD_DECLARE_ST(CMD_AT, cmd_at),        /* Auto sense */
        ATQ_CMD_DECLARE_ST(CMD_AT_Z, cmd_z),       /* Restore default settings */
        ATQ_CMD_DECLARE_ST(CMD_AT_E, cmd_ate0),    /* Disable echo */
        ATQ_CMD_DECLARE_ST(CMD_AT_CSCS, cmd_cscs), /* Set UCS-2 text encoding */

        ATQ_CMD_DECLARE_ST(CMD_AT_CGMI, cmd_cgmi),      /* Getting manufacturer info */
        ATQ_CMD_DECLARE_ST(CMD_AT_CGMM, cmd_cgmm),      /* Get Product name */
        ATQ_CMD_DECLARE_ST(CMD_AT_CGMR, cmd_cgmr),      /* Get software version */
        ATQ_CMD_DECLARE_ST(CMD_AT_CMEE, cmd_cmee),      /* Set MS Error Report to 'ERROR' only, TODO: change to 1 or 2 and add support in response handlers */
        ATQ_CMD_DECLARE_ST(CMD_AT_CGSN, cmd_cgsn),      /* IMEI Read */
        ATQ_CMD_DECLARE_ST(CMD_AT_CIMI, cmd_cimi),      /* IMSI Read */
        ATQ_CMD_DECLARE_ST(CMD_AT_CPIN, cmd_cpin),      /* Check is password authentication requirement and the remainder validation times */
        ATQ_CMD_DECLARE_ST(CMD_AT_COPS_INIT, cmd_cops), /* Read operator name */

        ATQ_CMD_DECLARE_STI(CMD_AT_CREG_INIT, cmd_creg_2), /* GSM registration status setting */
        ATQ_CMD_DECLARE_ST(CMD_AT_CREG, cmd_creg),         /* GSM registration status */

        ATQ_CMD_DECLARE_STI(CMD_AT_CNUM, cmd_cnum), /* Get Subscriber number */

        ATQ_CMD_DECLARE_ST(CMD_AT_CSCA, cmd_csca), /* Get SMS Service center address */

        ATQ_CMD_DECLARE_ST(CMD_AT_CSSN, cmd_cssn), /* Activate Supplementary Service Notification with CSSI and CSSU */
        ATQ_CMD_DECLARE_ST(CMD_AT_CMGF, cmd_cmgf), /* Set Message Format */

        ATQ_CMD_DECLARE_DYN(CMD_AT_CNMI), /* SMS Event Reporting Configuration */
        ATQ_CMD_DECLARE_DYN(CMD_AT_CPMS), /* SMS Storage Selection */
        ATQ_CMD_DECLARE_DYN(CMD_AT_CSMS), /* Select Message Service */
        ATQ_CMD_DECLARE_DYNI(CMD_AT_VTD), /* Set tone duration */
    };

    unsigned in, out;
    pvt_t* pvt = cpvt->pvt;
    at_queue_cmd_t cmds[ITEMS_OF(st_cmds)];
    at_queue_cmd_t dyn_cmd;
    int dyn_handled;

    /* customize list */
    for (in = out = 0; in < ITEMS_OF(st_cmds); ++in) {
        if (st_cmds[in].cmd == CMD_AT_Z && !CONF_SHARED(pvt, resetquectel)) {
            continue;
        }

        if (!(st_cmds[in].flags & ATQ_CMD_FLAG_STATIC)) {
            dyn_cmd     = st_cmds[in];
            dyn_handled = 0;

            if (st_cmds[in].cmd == CMD_AT_CPMS) {
                if (CONF_SHARED(pvt, msg_storage) == MESSAGE_STORAGE_AUTO) {
                    continue;
                }

                const char* const stor = dc_msgstor2str(CONF_SHARED(pvt, msg_storage));
                if (at_fill_generic_cmd(&dyn_cmd, "AT+CPMS=\"%s\",\"%s\",\"%s\"\r", stor, stor, stor)) {
                    ast_log(LOG_ERROR, "[%s] Device initialization - unable to create AT+CPMS command\n", PVT_ID(pvt));
                    continue;
                }
                dyn_handled = 1;
            }

            if (st_cmds[in].cmd == CMD_AT_CNMI) {
                int err = 0;
                if (!CONF_SHARED(pvt, msg_direct)) {
                    continue;
                }

                if (CONF_SHARED(pvt, msg_direct) > 0) {
                    err = at_fill_generic_cmd(&dyn_cmd, "AT+CNMI=2,2,2,0,%d\r", CONF_SHARED(pvt, resetquectel) ? 1 : 0);
                } else {
                    err = at_fill_generic_cmd(&dyn_cmd, "AT+CNMI=2,1,0,2,%d\r", CONF_SHARED(pvt, resetquectel) ? 1 : 0);
                }

                if (err) {
                    ast_log(LOG_ERROR, "[%s] Device initialization - unable to create AT+CNMI command\n", PVT_ID(pvt));
                    continue;
                }

                dyn_handled = 1;
            }

            if (st_cmds[in].cmd == CMD_AT_CSMS) {
                if (CONF_SHARED(pvt, msg_service) < 0) {
                    continue;
                }

                if (at_fill_generic_cmd(&dyn_cmd, "AT+CSMS=%d\r", CONF_SHARED(pvt, msg_service))) {
                    ast_log(LOG_ERROR, "[%s] Device initialization - unable to create AT+CSMS command\n", PVT_ID(pvt));
                    continue;
                }

                dyn_handled = 1;
            }

            if (st_cmds[in].cmd == CMD_AT_VTD) {
                if (CONF_SHARED(pvt, dtmf_duration) < 0) {
                    continue;
                }

                const int vtd = (int)(CONF_SHARED(pvt, dtmf_duration) / 100l);
                if (at_fill_generic_cmd(&dyn_cmd, "AT+VTD=%d\r", vtd)) {
                    ast_log(LOG_ERROR, "[%s] Device initialization - unable to create AT+VTD command\n", PVT_ID(pvt));
                    continue;
                }

                dyn_handled = 1;
            }
        }

        if (st_cmds[in].flags & ATQ_CMD_FLAG_STATIC) {
            cmds[out] = st_cmds[in];
        } else {
            if (dyn_handled) {
                cmds[out] = dyn_cmd;
            } else {
                ast_log(LOG_ERROR, "[%s] Device initialization - unhanled dynamic command: [%u] %s\n", PVT_ID(pvt), in, at_cmd2str(dyn_cmd.cmd));
                continue;
            }
        }
        out++;
    }

    if (out > 0) {
        return at_queue_insert(cpvt, cmds, out, 0);
    }
    return 0;
}

int at_enqueue_initialization_quectel(struct cpvt* cpvt, unsigned int dsci)
{
    static const char cmd_qpcmv[] = "AT+QPCMV?\r";

    static const char cmd_at_qindcfg_cc[]     = "AT+QINDCFG=\"ccinfo\",1,0\r";
    static const char cmd_at_qindcfg_cc_off[] = "AT+QINDCFG=\"ccinfo\",0,0\r";

    static const char cmd_at_dsci[]     = "AT^DSCI=1\r";
    static const char cmd_at_dsci_off[] = "AT^DSCI=0\r";

    static const char cmd_at_qindcfg_csq[]  = "AT+QINDCFG=\"csq\",1,0\r";
    static const char cmd_at_qindcfg_act[]  = "AT+QINDCFG=\"act\",1,0\r";
    static const char cmd_at_qindcfg_ring[] = "AT+QINDCFG=\"ring\",0,0\r";

    static const char cmd_at_qtonedet_0[] = "AT+QTONEDET=0\r";
    static const char cmd_at_qtonedet_1[] = "AT+QTONEDET=1\r";

    static const char cmd_at_qccid[] = "AT+QCCID\r";

    static const at_queue_cmd_t ccinfo_cmds[] = {
        ATQ_CMD_DECLARE_ST(CMD_AT_QINDCFG_CC, cmd_at_qindcfg_cc),
        ATQ_CMD_DECLARE_ST(CMD_AT_DSCI, cmd_at_dsci),
        ATQ_CMD_DECLARE_STI(CMD_AT_QINDCFG_CC_OFF, cmd_at_qindcfg_cc_off),
        ATQ_CMD_DECLARE_STI(CMD_AT_DSCI_OFF, cmd_at_dsci_off),
    };

    static const at_queue_cmd_t tonedet_cmds[] = {
        ATQ_CMD_DECLARE_STI(CMD_AT_QTONEDET_0, cmd_at_qtonedet_0),
        ATQ_CMD_DECLARE_STI(CMD_AT_QTONEDET_1, cmd_at_qtonedet_1),
    };

    static const char cmd_cereg_init[] = "AT+CEREG=2\r";

    struct pvt* const pvt   = cpvt->pvt;
    const unsigned int dtmf = CONF_SHARED(pvt, dtmf);

    const at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_STI(CMD_AT_QCCID, cmd_at_qccid),
        ATQ_CMD_DECLARE_STI(CMD_AT_QTONEDET_1, cmd_at_qtonedet_1),
        ATQ_CMD_DECLARE_ST(CMD_AT_CVOICE, cmd_qpcmv), /* read the current voice mode, and return sampling rate、data bit、frame period */
        ccinfo_cmds[dsci ? 2 : 3],
        ccinfo_cmds[dsci ? 1 : 0],
        ATQ_CMD_DECLARE_ST(CMD_AT_QINDCFG_CSQ, cmd_at_qindcfg_csq),
        ATQ_CMD_DECLARE_ST(CMD_AT_QINDCFG_ACT, cmd_at_qindcfg_act),
        ATQ_CMD_DECLARE_ST(CMD_AT_QINDCFG_RING, cmd_at_qindcfg_ring),
        tonedet_cmds[dtmf ? 1 : 0],
        ATQ_CMD_DECLARE_ST(CMD_AT_CEREG_INIT, cmd_cereg_init),
        ATQ_CMD_DECLARE_ST(CMD_AT_FINAL, cmd_at),
    };

    return at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 0);
}

int at_enqueue_initialization_simcom(struct cpvt* cpvt)
{
    static const char cmd_at_ccid[]   = "AT+CCID\r";
    static const char cmd_at_ciccid[] = "AT+CICCID\r";

    static const char cmd_cpcmreg[]      = "AT+CPCMREG?\r";
    static const char cmd_clcc_1[]       = "AT+CLCC=1\r";
    static const char cmd_cnsmod_1[]     = "AT+CNSMOD=1\r";
    static const char cmd_creg_init[]    = "AT+CREG=2\r";
    static const char cmd_autocsq_init[] = "AT+AUTOCSQ=1,1\r";
    static const char cmd_exunsol_init[] = "AT+EXUNSOL=\"SQ\",1\r";
    static const char cmd_clts_init[]    = "AT+CLTS=1\r";

    static const char cmd_at_ddet_0[] = "AT+DDET=0\r";
    static const char cmd_at_ddet_1[] = "AT+DDET=1\r";

    static const at_queue_cmd_t ddet_cmds[] = {
        ATQ_CMD_DECLARE_STI(CMD_AT_DDET_0, cmd_at_ddet_0),
        ATQ_CMD_DECLARE_STI(CMD_AT_DDET_1, cmd_at_ddet_1),
    };

    struct pvt* const pvt   = cpvt->pvt;
    const unsigned int dtmf = CONF_SHARED(pvt, dtmf);

    const at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_STI(CMD_AT_CCID, cmd_at_ccid),
        ATQ_CMD_DECLARE_STI(CMD_AT_CICCID, cmd_at_ciccid),
        ATQ_CMD_DECLARE_STI(CMD_AT_CPCMREG, cmd_cpcmreg),
        ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc_1),
        ATQ_CMD_DECLARE_ST(CMD_AT_CREG_INIT, cmd_creg_init),
        ATQ_CMD_DECLARE_STI(CMD_AT_CNSMOD_1, cmd_cnsmod_1),
        ATQ_CMD_DECLARE_STI(CMD_AT_AUTOCSQ_INIT, cmd_autocsq_init),
        ATQ_CMD_DECLARE_STI(CMD_AT_EXUNSOL_INIT, cmd_exunsol_init),
        ATQ_CMD_DECLARE_STI(CMD_AT_CLTS_INIT, cmd_clts_init),
        ddet_cmds[dtmf ? 1 : 0],
        ATQ_CMD_DECLARE_ST(CMD_AT_FINAL, cmd_at),
    };

    return at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 0);
}

int at_enqueue_initialization_other(struct cpvt* cpvt)
{
    static const at_queue_cmd_t cmds[] = {ATQ_CMD_DECLARE_ST(CMD_AT_FINAL, cmd_at)};

    return at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 0);
}

/*!
 * \brief Enqueue the AT+COPS? command
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

int at_enqueue_cspn_cops(struct cpvt* cpvt)
{
    static const char cmd_cspn[]    = "AT+CSPN?\r";
    static const char cmd_cops[]    = "AT+COPS?\r";
    static at_queue_cmd_t at_cmds[] = {ATQ_CMD_DECLARE_STI(CMD_AT_CSPN, cmd_cspn), ATQ_CMD_DECLARE_STI(CMD_AT_COPS, cmd_cops)};

    if (at_queue_insert_const(cpvt, at_cmds, ITEMS_OF(at_cmds), 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_qspn_qnwinfo(struct cpvt* cpvt)
{
    static const char cmd_qspn[]    = "+QSPN";
    static const char cmd_qnwinfo[] = "+QNWINFO";

    static at_queue_cmd_t at_cmds[] = {
        ATQ_CMD_DECLARE_STI(CMD_AT_QSPN, cmd_qspn),
        ATQ_CMD_DECLARE_STI(CMD_AT, cmd_qnwinfo),
    };

    if (at_queue_insert_const_at_once(cpvt, at_cmds, ITEMS_OF(at_cmds), 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/* SMS sending */
static int at_enqueue_pdu(const char* pdu, size_t length, size_t tpdulen, at_queue_cmd_t* cmds)
{
    static const at_queue_cmd_t at_cmds[] = {
        {CMD_AT_CMGS,    RES_SMS_PROMPT, ATQ_CMD_FLAG_DEFAULT, {ATQ_CMD_TIMEOUT_MEDIUM, 0}, NULL, 0},
        {CMD_AT_SMSTEXT, RES_OK,         ATQ_CMD_FLAG_DEFAULT, {ATQ_CMD_TIMEOUT_LONG, 0},   NULL, 0},
    };

    // DATA
    cmds[1]     = at_cmds[1];
    char* cdata = cmds[1].data = ast_malloc(length + 2);
    if (!cdata) {
        return -ENOMEM;
    }

    cmds[1].length = length + 1;
    memcpy(cdata, pdu, length);
    cdata[length]     = 0x1A;
    cdata[length + 1] = 0x0;

    // AT
    cmds[0] = at_cmds[0];
    if (at_fill_generic_cmd(&cmds[0], "AT+CMGS=%d\r", (int)tpdulen)) {
        at_queue_free_data(&cmds[1]);
        return E_CMD_FORMAT;
    }

    return 0;
}

static void clear_pdus(at_queue_cmd_t* cmds, ssize_t idx)
{
    if (idx <= 0) {
        return;
    }

    const ssize_t u = idx * 2;
    for (ssize_t i = 0; i < u; ++i) {
        ast_free(cmds[i].data);
    }
}

/*!
 * \brief Enqueue a SMS message(s)
 * \param cpvt -- cpvt structure
 * \param number -- the destination of the message
 * \param msg -- utf-8 encoded message
 */
int at_enqueue_sms(struct cpvt* cpvt, const char* destination, const char* msg, unsigned validity_minutes, int report_req, const char* payload,
                   size_t payload_len)
{
    struct pvt* const pvt = cpvt->pvt;

    /* set default validity period */
    if (validity_minutes <= 0) {
        validity_minutes = 3 * 24 * 60;
    }

    const int msg_len = strlen(msg);
    uint16_t msg_ucs2[msg_len * 2];
    ssize_t res = utf8_to_ucs2(msg, msg_len, msg_ucs2, sizeof(msg_ucs2));
    if (res < 0) {
        chan_quectel_err = E_PARSE_UTF8;
        return -1;
    }

    const int csmsref = smsdb_get_refid(pvt->imsi, destination);
    if (csmsref < 0) {
        chan_quectel_err = E_SMSDB;
        return -1;
    }

    pdu_part_t pdus[255];
    res = pdu_build_mult(pdus, "" /* pvt->sms_scenter */, destination, msg_ucs2, res, validity_minutes, !!report_req, csmsref);
    if (res < 0) {
        /* pdu_build_mult sets chan_quectel_err */
        return -1;
    }

    const int uid = smsdb_outgoing_add(pvt->imsi, destination, res, validity_minutes * 60, report_req, payload, payload_len);
    if (uid < 0) {
        chan_quectel_err = E_SMSDB;
        return -1;
    }

    at_queue_cmd_t cmds[res * 2];
    at_queue_cmd_t* pcmds = cmds;
    char hexbuf[PDU_LENGTH * 2 + 1];

    for (ssize_t i = 0; i < res; ++i, pcmds += 2) {
        hexify(pdus[i].buffer, pdus[i].length, hexbuf);
        if (at_enqueue_pdu(hexbuf, pdus[i].length * 2, pdus[i].tpdu_length, pcmds) < 0) {
            clear_pdus(cmds, i);
            return -1;
        }
    }

    if (at_queue_insert_uid(cpvt, cmds, res * 2, 0, uid)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    if (res <= 1) {
        ast_verb(1, "[%s][SMS:%d] Message enqueued\n", PVT_ID(pvt), uid);
    } else {
        ast_verb(1, "[%s][SMS:%d] Message enqueued [%d parts]\n", PVT_ID(pvt), uid, (int)res);
    }
    return 0;
}

/*!
 * \brief Enqueue AT+CUSD.
 * \param cpvt -- cpvt structure
 * \param code the CUSD code to send
 */

int at_enqueue_ussd(struct cpvt* cpvt, const char* code, int gsm7)
{
    static const char cmd[]     = "AT+CUSD=1,\"";
    static const char cmd_end[] = "\",15\r";

    at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_CUSD);
    ssize_t res;
    int length;
    char buf[4096];

    memcpy(buf, cmd, STRLEN(cmd));
    length       = STRLEN(cmd);
    int code_len = strlen(code);

    // use 7 bit encoding. 15 is 00001111 in binary and means 'Language using the GSM 7 bit default alphabet; Language
    // unspecified' accodring to GSM 23.038
    uint16_t code16[code_len * 2];
    res = utf8_to_ucs2(code, code_len, code16, sizeof(code16));
    if (res < 0) {
        chan_quectel_err = E_PARSE_UTF8;
        return -1;
    }
    if (gsm7) {
        uint8_t code_packed[4069];

        res = gsm7_encode(code16, res, code16);
        if (res < 0) {
            chan_quectel_err = E_ENCODE_GSM7;
            return -1;
        }
        res = gsm7_pack(code16, res, (char*)code_packed, sizeof(code_packed), 0);
        if (res < 0) {
            chan_quectel_err = E_PACK_GSM7;
            return -1;
        }
        res = (res + 1) / 2;
        hexify(code_packed, res, buf + STRLEN(cmd));
        length += res * 2;
    } else {
        hexify((const uint8_t*)code16, res * 2, buf + STRLEN(cmd));
        length += res * 4;
    }

    memcpy(buf + length, cmd_end, STRLEN(cmd_end) + 1);
    length += STRLEN(cmd_end);

    at_cmd.length = length;
    at_cmd.data   = ast_strdup(buf);
    if (!at_cmd.data) {
        chan_quectel_err = E_UNKNOWN;
        return -1;
    }

    if (at_queue_insert(cpvt, &at_cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue a DTMF command
 * \param cpvt -- cpvt structure
 * \param digit -- the dtmf digit to send
 * \return -2 if digis is invalid, 0 on success
 */

int at_enqueue_dtmf(struct cpvt* cpvt, char digit)
{
    switch (digit) {
        case 'a':
        case 'b':
        case 'c':
        case 'd': {
            char d = 'A';
            d += digit - 'a';
            return at_enqueue_generic(cpvt, CMD_AT_DTMF, 1, "AT+VTS=\"%c\"\r", d);
        }

        case 'A':
        case 'B':
        case 'C':
        case 'D':

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':

        case '*':
        case '#':
            return at_enqueue_generic(cpvt, CMD_AT_DTMF, 1, "AT+VTS=\"%c\"\r", digit);
    }
    return -1;
}

/*!
 * \brief Enqueue the AT+CCWA command (configure call waiting)
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

int at_enqueue_set_ccwa(struct cpvt* cpvt, unsigned call_waiting)
{
    static const char cmd_ccwa_get[] = "AT+CCWA=1,2,1\r";
    static const char cmd_ccwa_set[] = "AT+CCWA=%d,%d,%d\r";

    call_waiting_t value;
    at_queue_cmd_t cmds[] = {
        /* Set Call-Waiting On/Off */
        ATQ_CMD_DECLARE_DYNIT(CMD_AT_CCWA_SET, ATQ_CMD_TIMEOUT_MEDIUM, 0),
        /* Query CCWA Status for Voice Call  */
        ATQ_CMD_DECLARE_STIT(CMD_AT_CCWA_STATUS, cmd_ccwa_get, ATQ_CMD_TIMEOUT_MEDIUM, 0),
    };
    at_queue_cmd_t* pcmd = cmds;
    unsigned count       = ITEMS_OF(cmds);

    if (call_waiting == CALL_WAITING_DISALLOWED || call_waiting == CALL_WAITING_ALLOWED) {
        value         = call_waiting;
        const int err = call_waiting == CALL_WAITING_ALLOWED ? 1 : 0;

        if (at_fill_generic_cmd(&cmds[0], cmd_ccwa_set, err, err, CCWA_CLASS_VOICE)) {
            chan_quectel_err = E_CMD_FORMAT;
            return -1;
        }
    } else {
        value = CALL_WAITING_AUTO;
        pcmd++;
        count--;
    }
    CONF_SHARED(cpvt->pvt, callwaiting) = value;

    if (at_queue_insert(cpvt, pcmd, count, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue the device reset command (AT+CFUN Operation Mode Setting)
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

int at_enqueue_reset(struct cpvt* cpvt)
{
    static const char cmd[]            = "AT+CFUN=1,1\r";
    static const at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CFUN, cmd);

    if (at_queue_insert_const(cpvt, &at_cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue a dial commands
 * \param cpvt -- cpvt structure
 * \param number -- the called number
 * \param clir -- value of clir
 * \return 0 on success
 */
int at_enqueue_dial(struct cpvt* cpvt, const char* number, int clir)
{
    struct pvt* const pvt = cpvt->pvt;
    int cmdsno            = 0;
    char* tmp             = NULL;

    at_queue_cmd_t cmds[6];

    if (PVT_STATE(pvt, chan_count[CALL_STATE_ACTIVE]) > 0 && CPVT_TEST_FLAG(cpvt, CALL_FLAG_HOLD_OTHER)) {
        ATQ_CMD_INIT_ST(cmds[cmdsno], CMD_AT_CHLD_2, cmd_chld2);
        /*  enable this cause response_clcc() see all calls are held and insert 'AT+CHLD=2'
            ATQ_CMD_INIT_ST(cmds[1], CMD_AT_CLCC, cmd_clcc);
        */
        cmdsno++;
    }

    if (clir != -1) {
        if (at_fill_generic_cmd(&cmds[cmdsno], "AT+CLIR=%d\r", clir)) {
            chan_quectel_err = E_CMD_FORMAT;
            return -1;
        }
        tmp = cmds[cmdsno].data;
        ATQ_CMD_INIT_DYNI(cmds[cmdsno], CMD_AT_CLIR);
        cmdsno++;
    }

    if (at_fill_generic_cmd(&cmds[cmdsno], "ATD%s;\r", number)) {
        ast_free(tmp);
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    ATQ_CMD_INIT_DYNI(cmds[cmdsno], CMD_AT_D);
    cmdsno++;

    if (at_queue_insert(cpvt, cmds, cmdsno, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    /* set CALL_FLAG_NEED_HANGUP early because ATD may be still in queue while local hangup called */
    CPVT_SET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
    return 0;
}

/*!
 * \brief Enqueue a answer commands
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
int at_enqueue_answer(struct cpvt* cpvt)
{
    at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_DYN(CMD_AT_A),
    };
    int count = ITEMS_OF(cmds);
    const char* cmd1;

    switch (cpvt->state) {
        case CALL_STATE_INCOMING:
            cmd1 = "ATA\r";
            break;

        case CALL_STATE_WAITING:
            cmds[0].cmd = CMD_AT_CHLD_2x;
            cmd1        = "AT+CHLD=2%d\r";
            /* no need CMD_AT_DDSETEX in this case? */
            count--;
            break;

        default:
            ast_log(LOG_ERROR, "[%s] Request answer for call idx %d with state '%s'\n", PVT_ID(cpvt->pvt), cpvt->call_idx, call_state2str(cpvt->state));
            chan_quectel_err = E_UNKNOWN;
            return -1;
    }

    if (at_fill_generic_cmd(&cmds[0], cmd1, cpvt->call_idx)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, cmds, count, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue an activate commands 'Put active calls on hold and activate call x.'
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
int at_enqueue_activate(struct cpvt* cpvt)
{
    if (cpvt->state == CALL_STATE_ACTIVE) {
        return 0;
    }

    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_CHLD_2x);

    if (cpvt->state != CALL_STATE_ONHOLD && cpvt->state != CALL_STATE_WAITING) {
        ast_log(LOG_ERROR, "[%s] Imposible activate call idx %d from state '%s'\n", PVT_ID(cpvt->pvt), cpvt->call_idx, call_state2str(cpvt->state));
        return -1;
    }

    if (at_fill_generic_cmd(&cmd, "AT+CHLD=2%d\r", cpvt->call_idx)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }
    return 0;
}

/*!
 * \brief Enqueue an commands for 'Put active calls on hold and activate the waiting or held call.'
 * \param pvt -- pvt structure
 * \return 0 on success
 */
int at_enqueue_flip_hold(struct cpvt* cpvt)
{
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CHLD_2, cmd_chld2);

    if (at_queue_insert_const(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

enum at_ping_method_t { PING_AT, PING_QUECTEL, PING_SIMCOM };

/*!
 * \brief Enqueue ping command
 * \param pvt -- pvt structure
 * \return 0 on success
 */
int at_enqueue_ping(struct cpvt* cpvt)
{
    struct pvt* const pvt          = cpvt->pvt;
    enum at_ping_method_t ping_cmd = PING_AT;

    if (CONF_SHARED(pvt, query_time)) {
        ping_cmd = pvt->is_simcom ? PING_SIMCOM : PING_QUECTEL;
    }

    switch (ping_cmd) {
        case PING_AT:
            return at_enqueue_at(cpvt);

        case PING_QUECTEL:
            return at_enqueue_qlts_1(cpvt);

        case PING_SIMCOM:
            return at_enqueue_cclk_query(cpvt);
    }

    return -1;
}

/*!
 * \brief Enqueue user-specified command
 * \param cpvt -- cpvt structure
 * \param input -- user's command
 * \return 0 on success
 */
int at_enqueue_user_cmd(struct cpvt* cpvt, const char* input)
{
    if (at_enqueue_generic(cpvt, CMD_USER, 1, "%s\r", input)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Start reading next SMS, if any
 * \param cpvt -- cpvt structure
 */
void at_sms_retrieved(struct cpvt* cpvt, int confirm)
{
    struct pvt* const pvt = cpvt->pvt;

    if (pvt->incoming_sms_index >= 0) {
        if (CONF_SHARED(pvt, autodeletesms)) {
            at_enqueue_delete_sms(cpvt, pvt->incoming_sms_index, TRIBOOL_NONE);
        }
        if (confirm) {
            ast_log(LOG_WARNING, "[%s][SMS:%d] Message not retrieved\n", PVT_ID(pvt), pvt->incoming_sms_index);
        }
    }

    pvt->incoming_sms_index = -1;
}

int at_enqueue_list_messages(struct cpvt* cpvt, enum msg_status_t stat)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_CMGL);

    if (at_fill_generic_cmd(&cmd, "AT+CMGL=%d\r", (int)stat)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue commands for reading SMS
 * \param cpvt -- cpvt structure
 * \param index -- index of message in store
 * \return 0 on success
 */
int at_enqueue_retrieve_sms(struct cpvt* cpvt, int idx)
{
    struct pvt* const pvt = cpvt->pvt;
    at_queue_cmd_t cmd    = ATQ_CMD_DECLARE_DYN(CMD_AT_CMGR);

    /* check if message is already being received */
    if (pvt->incoming_sms_index >= 0) {
        ast_debug(4, "[%s] SMS retrieve of [%d] already in progress\n", PVT_ID(pvt), pvt->incoming_sms_index);
        return 0;
    }

    pvt->incoming_sms_index = idx;

    if (at_fill_generic_cmd(&cmd, "AT+CMGR=%d\r", idx)) {
        chan_quectel_err = E_CMD_FORMAT;
        goto error;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        goto error;
    }

    return 0;

error:
    ast_log(LOG_WARNING, "[%s] Unable to read message %d\n", PVT_ID(pvt), idx);

    pvt->incoming_sms_index = -1;
    return -1;
}

int at_enqueue_cmgd(struct cpvt* cpvt, unsigned int idx, int delflag)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYNI(CMD_AT_CMGD);

    if (at_fill_generic_cmd(&cmd, "AT+CMGD=%u,%d\r", idx, delflag)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

static const char cmd_cnma[] = "AT+CNMA\r";

/*!
 * \brief Enqueue commands for deleting SMS
 * \param cpvt -- cpvt structure
 * \param index -- index of message in store
 * \return 0 on success
 */
int at_enqueue_delete_sms(struct cpvt* cpvt, int idx, tristate_bool_t ack)
{
    at_queue_cmd_t cmds[] = {ATQ_CMD_DECLARE_STI(CMD_AT_CNMA, cmd_cnma), ATQ_CMD_DECLARE_DYNI(CMD_AT_CMGD)};

    if (idx < 0) {
        return 0;
    }

    if (at_fill_generic_cmd(&cmds[1], "AT+CMGD=%d\r", idx)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmds[ack ? 0 : 1], ack ? 2 : 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_delete_sms_n(struct cpvt* cpvt, int idx, tristate_bool_t ack)
{
    at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_DYNI(CMD_AT_CNMA),
        ATQ_CMD_DECLARE_DYNI(CMD_AT_CMGD),
    };

    if (idx < 0) {
        return 0;
    }

    if (ack) {
        if (at_fill_generic_cmd(&cmds[0], "AT+CNMA=%d\r", (ack < 0) ? 2 : 1)) {
            chan_quectel_err = E_CMD_FORMAT;
            return -1;
        }
    }

    if (at_fill_generic_cmd(&cmds[1], "AT+CMGD=%d\r", idx)) {
        at_queue_free_data(cmds);
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmds[ack ? 0 : 1], ack ? 2 : 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_msg_direct(struct cpvt* cpvt, int directflag)
{
    static const char at_cnmi_direct[]   = "AT+CNMI=2,2,2,0,0\r";
    static const char at_cnmi_indirect[] = "AT+CNMI=2,1,0,2,0\r";

    static const at_queue_cmd_t cmd_direct   = ATQ_CMD_DECLARE_STI(CMD_AT_CNMI, at_cnmi_direct);
    static const at_queue_cmd_t cmd_indirect = ATQ_CMD_DECLARE_STI(CMD_AT_CNMI, at_cnmi_indirect);

    if (at_queue_insert_const(cpvt, directflag ? &cmd_direct : &cmd_indirect, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_msg_ack(struct cpvt* cpvt)
{
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_STI(CMD_AT_CNMA, cmd_cnma);

    if (at_queue_insert_const(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_msg_ack_n(struct cpvt* cpvt, int n, int uid)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYNI(CMD_AT_CNMA);

    if (at_fill_generic_cmd(&cmd, "AT+CNMA=%d\r", n)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert_uid(cpvt, &cmd, 1, 1, uid)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

#if 0
static int at_enqueue_msg_ack_n(struct cpvt *cpvt, int n, int uid)
{
	static const char ctrl_z[2] = { 0x1A, 0x00 };

	at_queue_cmd_t at_cmds[] = {
		{ CMD_AT_CNMA,    RES_SMS_PROMPT, ATQ_CMD_FLAG_DEFAULT, { ATQ_CMD_TIMEOUT_MEDIUM, 0}, NULL, 0 },
		{ CMD_AT_SMSTEXT, RES_OK,         ATQ_CMD_FLAG_STATIC, { ATQ_CMD_TIMEOUT_LONG, 0},   NULL, 0 }
	};

	if (at_fill_generic_cmd(&at_cmds[0], "AT+CNMA=%d,0\r", n)) {
		chan_quectel_err = E_CMD_FORMAT;
		return err;
	}

	at_cmds[1].length = 1;
	at_cmds[1].data = (void*)ctrl_z;

	if (at_queue_insert_uid(cpvt, at_cmds, ITEMS_OF(at_cmds), 1, uid)) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}

	return 0;
}
#endif

static int map_hangup_cause(int hangup_cause)
{
    switch (hangup_cause) {
        // list of supported cause codes
        case AST_CAUSE_UNALLOCATED:
        case AST_CAUSE_NORMAL_CLEARING:
        case AST_CAUSE_USER_BUSY:
        case AST_CAUSE_NO_USER_RESPONSE:
        case AST_CAUSE_CALL_REJECTED:
        case AST_CAUSE_DESTINATION_OUT_OF_ORDER:
        case AST_CAUSE_NORMAL_UNSPECIFIED:
        case AST_CAUSE_INCOMPATIBLE_DESTINATION:
            return hangup_cause;

        case AST_CAUSE_NO_ANSWER:
            return AST_CAUSE_NO_USER_RESPONSE;

        default:  // use default one
            return AST_CAUSE_NORMAL_CLEARING;
    }
}

static int at_enqueue_chup(struct cpvt* const cpvt)
{
    static const char cmd_chup[]        = "AT+CHUP\r";
    static const at_queue_cmd_t at_chup = ATQ_CMD_DECLARE_STFT(CMD_AT_CHUP, RES_OK, cmd_chup, ATQ_CMD_FLAG_DEFAULT, ATQ_CMD_TIMEOUT_LONG, 0);

    if (at_queue_insert_const(cpvt, &at_chup, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

static int at_enqueue_qhup(struct cpvt* const cpvt, int call_idx, int release_cause)
{
    // AT+QHUP=<cause>,<idx>
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYNFT(CMD_AT_QHUP, RES_OK, ATQ_CMD_FLAG_DEFAULT, ATQ_CMD_TIMEOUT_LONG, 0);

    if (at_fill_generic_cmd(&cmd, "AT+QHUP=%d,%d\r", map_hangup_cause(release_cause), call_idx)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue AT+CHLD1x or AT+CHUP hangup command
 * \param cpvt -- channel_pvt structure
 * \param call_idx -- call id
 * \return 0 on success
 */

int at_enqueue_hangup(struct cpvt* cpvt, int call_idx, int release_cause)
{
    struct pvt* const pvt = cpvt->pvt;

    if (cpvt == &pvt->sys_chan || cpvt->dir == CALL_DIR_INCOMING || (cpvt->state != CALL_STATE_INIT && cpvt->state != CALL_STATE_DIALING)) {
        /* FIXME: other channels may be in RELEASED or INIT state */
        if (PVT_STATE(pvt, chansno) > 1) {
            static const char cmd_chld1x[]  = "AT+CHLD=1%d\r";
            static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_STFT(CMD_AT_CHLD_1x, RES_OK, cmd_chld1x, ATQ_CMD_FLAG_DEFAULT, ATQ_CMD_TIMEOUT_LONG, 0);

            if (at_queue_insert_const(cpvt, &cmd, 1, 1)) {
                chan_quectel_err = E_QUEUE;
                return -1;
            }

            return 0;
        }
    }

    if (pvt->is_simcom) {  // AT+CHUP
        return at_enqueue_chup(cpvt);
    } else {
        return (CONF_SHARED(pvt, qhup)) ? at_enqueue_qhup(cpvt, call_idx, release_cause) : at_enqueue_chup(cpvt);
    }
}

/*!
 * \brief Enqueue AT+CLVL commands for volume synchronization
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

int at_enqueue_volsync(struct cpvt* cpvt)
{
    static const char cmd1[] = "AT+CLVL=1\r";
    static const char cmd2[] = "AT+CLVL=5\r";

    static const at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_ST(CMD_AT_CLVL, cmd1),
        ATQ_CMD_DECLARE_ST(CMD_AT_CLVL, cmd2),
    };

    if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue AT+CLCC command
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
int at_enqueue_clcc(struct cpvt* cpvt)
{
    static const char cmd_clcc[]       = "AT+CLCC\r";
    static const at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc);

    if (at_queue_insert_const(cpvt, &at_cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief Enqueue AT+CHLD=3 command
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
int at_enqueue_conference(struct cpvt* cpvt)
{
    static const char cmd_chld3[]   = "AT+CHLD=3\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CHLD_3, cmd_chld3);

    if (at_queue_insert_const(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

/*!
 * \brief SEND AT+CHUP command to device IMMEDIALITY
 * \param cpvt -- cpvt structure
 */
int at_hangup_immediately(struct cpvt* cpvt, int release_cause)
{
    struct pvt* pvt = cpvt->pvt;

    if (pvt->is_simcom) {  // AT+CHUP
        static const char cmd_chup[]    = "+CHUP";
        static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CHUP, cmd_chup);

        if (at_queue_add(cpvt, &cmd, 1, 0, 1u) == NULL) {
            chan_quectel_err = E_QUEUE;
            return -1;
        }
    } else {  // AT+QHUP=<cause>,<idx>
        at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_QHUP);

        if (at_fill_generic_cmd(&cmd, "+QHUP=%d,%d", map_hangup_cause(release_cause), cpvt->call_idx)) {
            chan_quectel_err = E_CMD_FORMAT;
            return -1;
        }

        if (at_queue_add(cpvt, &cmd, 1, 0, 1u) == NULL) {
            chan_quectel_err = E_QUEUE;
            return -1;
        }
    }

    return 0;
}

int at_disable_uac_immediately(struct pvt* pvt)
{
    static const char cmd_qpcmv[]   = "+QPCMV=0";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_QPCMV_0, cmd_qpcmv);

    if (at_queue_add(&pvt->sys_chan, &cmd, 1, 0, 1u) == NULL) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_mute(struct cpvt* cpvt, int mute)
{
    static const char cmd_cmut0[] = "AT+CMUT=0\r";
    static const char cmd_cmut1[] = "AT+CMUT=1\r";

    static const at_queue_cmd_t cmds0 = ATQ_CMD_DECLARE_ST(CMD_AT_CMUT_0, cmd_cmut0);
    static const at_queue_cmd_t cmds1 = ATQ_CMD_DECLARE_ST(CMD_AT_CMUT_1, cmd_cmut1);

    const at_queue_cmd_t* cmds = mute ? &cmds1 : &cmds0;

    if (at_queue_insert_const(cpvt, cmds, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_enable_tty(struct cpvt* cpvt)
{
    static const char cmd_atqpcmv[] = "AT+QPCMV=1,0\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_QPCMV_TTY, cmd_atqpcmv);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_enable_uac(struct cpvt* cpvt)
{
    static const char cmd_atqpcmv[] = "AT+QPCMV=1,2\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_QPCMV_UAC, cmd_atqpcmv);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_qlts(struct cpvt* cpvt, int mode)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_QLTS);

    if (at_fill_generic_cmd(&cmd, "AT+QLTS=%d\r", mode)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_qlts_1(struct cpvt* cpvt)
{
    static const char cmd_qlts[]    = "AT+QLTS=1\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_QLTS_1, cmd_qlts);

    if (at_queue_insert_const(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_cclk_query(struct cpvt* cpvt)
{
    static const char cmd_cclk[]    = "AT+CCLK?\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CCLK, cmd_cclk);

    if (at_queue_insert_const(cpvt, &cmd, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_query_qgains(struct cpvt* cpvt)
{
    static const char cmd_qmic[]    = "+QMIC?";
    static const char cmd_qrxgain[] = "+QRXGAIN?";

    static const at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_ST(CMD_AT_QMIC, cmd_qmic),
        ATQ_CMD_DECLARE_ST(CMD_AT_QRXGAIN, cmd_qrxgain),
    };

    if (at_queue_insert_const_at_once(cpvt, cmds, ITEMS_OF(cmds), 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_qgains(struct cpvt* cpvt, int txgain, int rxgain)
{
    int pos               = 0;
    int cnt               = 0;
    at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_DYN(CMD_AT_QMIC),
        ATQ_CMD_DECLARE_DYN(CMD_AT_QRXGAIN),
    };

    if (txgain >= 0) {
        if (at_fill_generic_cmd(&cmds[0], "+QMIC=%d", txgain)) {
            chan_quectel_err = E_CMD_FORMAT;
            return -1;
        }

        cnt += 1;
    } else {
        pos += 1;
    }

    if (rxgain >= 0) {
        if (at_fill_generic_cmd(&cmds[1], "+QRXGAIN=%d", rxgain)) {
            chan_quectel_err = E_CMD_FORMAT;
            return -1;
        }

        cnt += 1;
    }

    if (at_queue_add(cpvt, &cmds[pos], cnt, 0, 1u) == NULL) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_query_cgains(struct cpvt* cpvt)
{
    static const char cmd_coutgain[] = "+COUTGAIN?";
    static const char cmd_cmicgain[] = "+CMICGAIN?";

    static const at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_ST(CMD_AT_COUTGAIN, cmd_coutgain),
        ATQ_CMD_DECLARE_ST(CMD_AT_CMICGAIN, cmd_cmicgain),
    };

    if (at_queue_insert_const_at_once(cpvt, cmds, ITEMS_OF(cmds), 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_cgains(struct cpvt* cpvt, int txgain, int rxgain)
{
    int pos               = 0;
    int cnt               = 0;
    at_queue_cmd_t cmds[] = {
        ATQ_CMD_DECLARE_DYN(CMD_AT_COUTGAIN),
        ATQ_CMD_DECLARE_DYN(CMD_AT_CMICGAIN),
    };

    if (txgain >= 0) {
        if (at_fill_generic_cmd(&cmds[0], "+COUTGAIN=%d", txgain)) {
            chan_quectel_err = E_CMD_FORMAT;
            return -1;
        }

        cnt += 1;
    } else {
        pos += 1;
    }

    if (rxgain >= 0) {
        if (at_fill_generic_cmd(&cmds[1], "+CMICGAIN=%d", rxgain)) {
            chan_quectel_err = E_CMD_FORMAT;
            return -1;
        }

        cnt += 1;
    }

    if (at_queue_add(cpvt, &cmds[pos], cnt, 0, 1u) == NULL) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_query_qaudloop(struct cpvt* cpvt)
{
    static const char cmd_qaudloop[] = "AT+QAUDLOOP?\r";
    static const at_queue_cmd_t cmd  = ATQ_CMD_DECLARE_ST(CMD_AT_QAUDLOOP, cmd_qaudloop);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_qaudloop(struct cpvt* cpvt, int aloop)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_QAUDLOOP);

    if (at_fill_generic_cmd(&cmd, "AT+QAUDLOOP=%d\r", aloop)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_query_qaudmod(struct cpvt* cpvt)
{
    static const char cmd_qaudmod[] = "AT+QAUDMOD?\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_QAUDMOD, cmd_qaudmod);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_qaudmod(struct cpvt* cpvt, int amode)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_QAUDMOD);

    if (at_fill_generic_cmd(&cmd, "AT+QAUDMOD=%d\r", amode)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_query_qmic(struct cpvt* cpvt)
{
    static const char cmd_qmic[]    = "AT+QMIC?\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_QMIC, cmd_qmic);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_qmic(struct cpvt* cpvt, int gain)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_QMIC);

    if (at_fill_generic_cmd(&cmd, "AT+QMIC=%d\r", gain)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_query_cmicgain(struct cpvt* cpvt)
{
    static const char cmd_cmicgain[] = "AT+CMICGAIN?\r";
    static const at_queue_cmd_t cmd  = ATQ_CMD_DECLARE_ST(CMD_AT_CMICGAIN, cmd_cmicgain);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_cmicgain(struct cpvt* cpvt, int gain)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_CMICGAIN);

    if (at_fill_generic_cmd(&cmd, "AT+CMICGAIN=%d\r", gain)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_query_qrxgain(struct cpvt* cpvt)
{
    static const char cmd_qrxgain[] = "AT+QRXGAIN?\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_AT_QRXGAIN, cmd_qrxgain);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_qrxgain(struct cpvt* cpvt, int gain)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_QRXGAIN);

    if (at_fill_generic_cmd(&cmd, "AT+QRXGAIN=%d\r", gain)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_query_coutgain(struct cpvt* cpvt)
{
    static const char cmd_coutgain[] = "AT+COUTGAIN?\r";
    static const at_queue_cmd_t cmd  = ATQ_CMD_DECLARE_ST(CMD_AT_QRXGAIN, cmd_coutgain);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_coutgain(struct cpvt* cpvt, int gain)
{
    at_queue_cmd_t cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_COUTGAIN);

    if (at_fill_generic_cmd(&cmd, "AT+COUTGAIN=%d\r", gain)) {
        chan_quectel_err = E_CMD_FORMAT;
        return -1;
    }

    if (at_queue_insert(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_cpcmreg(struct cpvt* cpvt, int reg)
{
    static const char cmd_cpcmreg_0[] = "AT+CPCMREG=0\r";
    static const char cmd_cpcmreg_1[] = "AT+CPCMREG=1\r";

    static const at_queue_cmd_t cmd0 = ATQ_CMD_DECLARE_ST(CMD_AT_CPCMREG0, cmd_cpcmreg_0);
    static const at_queue_cmd_t cmd1 = ATQ_CMD_DECLARE_ST(CMD_AT_CPCMREG1, cmd_cpcmreg_1);

    if (at_queue_insert_const(cpvt, reg ? &cmd1 : &cmd0, 1, 1)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_cpcmreg_immediately(struct pvt* pvt, int reg)
{
    static const char cmd_cpcmreg_0[] = "+CPCMREG=0,0";
    static const char cmd_cpcmreg_1[] = "+CPCMREG=1";

    static const at_queue_cmd_t cmd0 = ATQ_CMD_DECLARE_ST(CMD_AT_CPCMREG0, cmd_cpcmreg_0);
    static const at_queue_cmd_t cmd1 = ATQ_CMD_DECLARE_ST(CMD_AT_CPCMREG1, cmd_cpcmreg_1);

    if (at_queue_add(&pvt->sys_chan, reg ? &cmd1 : &cmd0, 1, 0, 1u) == NULL) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_cpcmfrm(struct cpvt* cpvt, int frm)
{
    static const char cmd_cpcmfrm_0[] = "AT+CPCMFRM=0\r";
    static const char cmd_cpcmfrm_1[] = "AT+CPCMFRM=1\r";

    static const at_queue_cmd_t cmd0 = ATQ_CMD_DECLARE_ST(CMD_AT_CPCMFRM_8K, cmd_cpcmfrm_0);
    static const at_queue_cmd_t cmd1 = ATQ_CMD_DECLARE_ST(CMD_AT_CPCMFRM_16K, cmd_cpcmfrm_1);

    if (at_queue_insert_const(cpvt, frm ? &cmd1 : &cmd0, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_csq(struct cpvt* cpvt)
{
    static const char cmd_at_csq[]  = "AT+CSQ\r";
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_STI(CMD_AT_CSQ, cmd_at_csq);

    if (at_queue_insert_const(cpvt, &cmd, 1, 0)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}

int at_enqueue_escape(struct cpvt* cpvt, int uid)
{
    static const char cmd_esc[2]    = {0x1B, 0x00};
    static const at_queue_cmd_t cmd = ATQ_CMD_DECLARE_ST(CMD_ESC, cmd_esc);

    if (at_queue_insert_uid(cpvt, (at_queue_cmd_t*)&cmd, 1, 1, uid)) {
        chan_quectel_err = E_QUEUE;
        return -1;
    }

    return 0;
}
