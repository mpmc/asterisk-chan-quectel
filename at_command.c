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
//#include ".h"
#include "at_queue.h"
#include "char_conv.h"			/* char_to_hexstr_7bit() */
#include "chan_quectel.h"		/* struct pvt */
#include "pdu.h"			/* build_pdu() */
#include "smsdb.h"
#include "error.h"

static const char cmd_at[] 	 = "AT\r";
static const char cmd_chld1x[]   = "AT+CHLD=1%d\r";
static const char cmd_chld2[]    = "AT+CHLD=2\r";
static const char cmd_clcc[]     = "AT+CLCC\r";

/*!
 * \brief Get the string representation of the given AT command
 * \param cmd -- the command to process
 * \return a string describing the given command
 */

const char* at_cmd2str(at_cmd_t cmd)
{
	static const char * const cmds[] = {
		AT_COMMANDS_TABLE(AT_CMD_AS_STRING)
	};
	return enum2str_def(cmd, cmds, ITEMS_OF(cmds), "UNDEFINED");
}

/*!
 * \brief Format and fill generic command
 * \param cmd -- the command structure
 * \param format -- printf format string
 * \param ap -- list of arguments
 * \return 0 on success
 */

static int at_fill_generic_cmd_va (at_queue_cmd_t * cmd, const char * format, va_list ap)
{
	char buf[4096];

	cmd->length = vsnprintf (buf, sizeof(buf)-1, format, ap);

	buf[cmd->length] = 0;
	cmd->data = ast_strdup(buf);
	if(!cmd->data)
		return -1;

	cmd->flags &= ~ATQ_CMD_FLAG_STATIC;
	return 0;

}

/*!
 * \brief Format and fill generic command
 * \param cmd -- the command structure
 * \param format -- printf format string
 * \return 0 on success
 */

static int __attribute__ ((format(printf, 2, 3))) at_fill_generic_cmd (at_queue_cmd_t * cmd, const char * format, ...)
{
	va_list ap;
	int rv;

	va_start(ap, format);
	rv = at_fill_generic_cmd_va(cmd, format, ap);
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

static int __attribute__ ((format(printf, 4, 5))) at_enqueue_generic(struct cpvt *cpvt, at_cmd_t cmd, int prio, const char *format, ...)
{
	va_list ap;
	int rv;
	at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_DYN(cmd);

	va_start(ap, format);
	rv = at_fill_generic_cmd_va(&at_cmd, format, ap);
	va_end(ap);

	if(!rv)
		rv = at_queue_insert(cpvt, &at_cmd, 1, prio);
	return rv;
}

/*!
 * \brief Enqueue initialization commands
 * \param cpvt -- cpvt structure
 * \param from_command -- begin initialization from this command in list
 * \return 0 on success
 */
int at_enqueue_initialization(struct cpvt *cpvt, at_cmd_t from_command)
{
	static const char cmd_z[] = "ATZ\r";
	static const char cmd_ate0[] = "ATE0\r";

	static const char cmd_cgmi[] = "AT+CGMI\r";
	static const char cmd_csca[] = "AT+CSCA?\r";
	static const char cmd_cgmm[] = "AT+CGMM\r";
	static const char cmd_cgmr[] = "AT+CGMR\r";

	static const char cmd_cmee[] = "AT+CMEE=0\r";
	static const char cmd_cgsn[] = "AT+CGSN\r";
	static const char cmd_cimi[] = "AT+CIMI\r";
	static const char cmd_cpin[] = "AT+CPIN?\r";

	static const char cmd_cops[] = "AT+COPS=0,0\r";
	static const char cmd_creg_2[] = "AT+CREG=2\r";
	static const char cmd_creg[] = "AT+CREG?\r";
	static const char cmd_cnum[] = "AT+CNUM\r";

	static const char cmd_qpcmv[] = "AT+QPCMV?\r";
	static const char cmd_cpcmreg[] = "AT+CPCMREG?\r";
//	static const char cmd_clip[] = "AT+CLIP=0\r";
	static const char cmd_cssn[] = "AT+CSSN=1,1\r";
	static const char cmd_cmgf[] = "AT+CMGF=0\r";
	static const char cmd_cscs[] = "AT+CSCS=\"UCS2\"\r";

	static const char cmd_cpms[] = "AT+CPMS=\"SM\",\"SM\",\"SM\"\r";
	static const char cmd_cnmi[] = "AT+CNMI=2,1,0,2,0\r";

	static const char cmd_at_qindcfg_csq[] = "AT+QINDCFG=\"csq\",1,0\r";
	static const char cmd_at_qindcfg_act[] = "AT+QINDCFG=\"act\",1,0\r";
	static const char cmd_dsci[] = "AT^DSCI=1\r";

	static const at_queue_cmd_t st_cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT, cmd_at),
		ATQ_CMD_DECLARE_ST(CMD_AT_E, cmd_ate0),			/* Disable echo */

		ATQ_CMD_DECLARE_ST(CMD_AT_Z, cmd_z),			/* Restore default settings */

		ATQ_CMD_DECLARE_ST(CMD_AT_CGMI, cmd_cgmi),		/* Getting manufacturer info */
		ATQ_CMD_DECLARE_ST(CMD_AT_CGMM, cmd_cgmm),		/* Get Product name */
		ATQ_CMD_DECLARE_ST(CMD_AT_CGMR, cmd_cgmr),		/* Get software version */
		ATQ_CMD_DECLARE_ST(CMD_AT_CMEE, cmd_cmee),		/* set MS Error Report to 'ERROR' only  TODO: change to 1 or 2 and add support in response handlers */
		ATQ_CMD_DECLARE_ST(CMD_AT_CGSN, cmd_cgsn),		/* IMEI Read */
		ATQ_CMD_DECLARE_ST(CMD_AT_CIMI, cmd_cimi),		/* IMSI Read */
		ATQ_CMD_DECLARE_ST(CMD_AT_CPIN, cmd_cpin),		/* check is password authentication requirement and the remainder validation times */
		ATQ_CMD_DECLARE_ST(CMD_AT_COPS_INIT, cmd_cops),/* Read operator name */

		ATQ_CMD_DECLARE_STI(CMD_AT_CREG_INIT,cmd_creg_2),/* GSM registration status setting */
		ATQ_CMD_DECLARE_ST(CMD_AT_CREG, cmd_creg),		/* GSM registration status */

		ATQ_CMD_DECLARE_STI(CMD_AT_CNUM, cmd_cnum),		/* Get Subscriber number */
		ATQ_CMD_DECLARE_STI(CMD_AT_CVOICE, cmd_qpcmv),	/* read the current voice mode, and return sampling rate、data bit、frame period */
		ATQ_CMD_DECLARE_STI(CMD_AT_CVOICE2, cmd_cpcmreg),

		ATQ_CMD_DECLARE_STI(CMD_AT_CSCS, cmd_cscs),		/* UCS-2 text encoding */
		ATQ_CMD_DECLARE_ST(CMD_AT_CSCA, cmd_csca),		/* Get SMS Service center address */

//		ATQ_CMD_DECLARE_ST(CMD_AT_CLIP, cmd_clip),		/* disable  Calling line identification presentation in unsolicited response +CLIP: <number>,<type>[,<subaddr>,<satype>[,[<alpha>][,<CLI validitity>]] */
		ATQ_CMD_DECLARE_ST(CMD_AT_CSSN, cmd_cssn),		/* activate Supplementary Service Notification with CSSI and CSSU */
		ATQ_CMD_DECLARE_ST(CMD_AT_CMGF, cmd_cmgf),		/* Set Message Format */

		ATQ_CMD_DECLARE_ST(CMD_AT_CPMS, cmd_cpms),		/* SMS Storage Selection */

		/* pvt->initialized = 1 after successful of CMD_AT_CNMI */
		ATQ_CMD_DECLARE_ST(CMD_AT_CNMI, cmd_cnmi),		/* New SMS Notification Setting +CNMI=[<mode>[,<mt>[,<bm>[,<ds>[,<bfr>]]]]] */
		ATQ_CMD_DECLARE_ST(CMD_AT_DSCI, cmd_dsci),		/* Enable call status notifications */
		ATQ_CMD_DECLARE_STI(CMD_AT_QINDCFG_CSQ, cmd_at_qindcfg_csq),
		ATQ_CMD_DECLARE_STI(CMD_AT_QINDCFG_ACT, cmd_at_qindcfg_act),

	};

	unsigned in, out;
	int begin = -1;
	pvt_t * pvt = cpvt->pvt;
	at_queue_cmd_t cmds[ITEMS_OF(st_cmds)];

	/* customize list */
	for(in = out = 0; in < ITEMS_OF(st_cmds); ++in) {
		if(begin == -1) {
			if(st_cmds[in].cmd == from_command)
				begin = in;
			else
				continue;
		}

		if(st_cmds[in].cmd == CMD_AT_Z && !CONF_SHARED(pvt, resetquectel))
			continue;

		memcpy(&cmds[out], &st_cmds[in], sizeof(st_cmds[in]));
		if(cmds[out].cmd == from_command) begin = out;
		out++;
	}

	if(out > 0) return at_queue_insert(cpvt, cmds, out, 0);
	return 0;
}

/*!
 * \brief Enqueue the AT+COPS? command
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

int at_enqueue_cops(struct cpvt *cpvt)
{
	static const char cmd[] = "AT+COPS?\r";
	static at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_ST(CMD_AT_COPS, cmd);

	if (at_queue_insert_const(cpvt, &at_cmd, 1, 0) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}
	return 0;
}


/* SMS sending */
static int at_enqueue_pdu(struct cpvt *cpvt, const char *pdu, size_t length, size_t tpdulen, int uid)
{
	char buf[8+25+1];
	at_queue_cmd_t at_cmd[] = {
		{ CMD_AT_CMGS,    RES_SMS_PROMPT, ATQ_CMD_FLAG_DEFAULT, { ATQ_CMD_TIMEOUT_MEDIUM, 0}, NULL, 0 },
		{ CMD_AT_SMSTEXT, RES_OK,         ATQ_CMD_FLAG_DEFAULT, { ATQ_CMD_TIMEOUT_LONG, 0},   NULL, 0 }
		};

	at_cmd[1].data = ast_malloc(length + 2);
	if(!at_cmd[1].data)
	{
		return -ENOMEM;
	}

	at_cmd[1].length = length + 1;

	memcpy(at_cmd[1].data, pdu, length);
	at_cmd[1].data[length] = 0x1A;
	at_cmd[1].data[length + 1] = 0x0;

	at_cmd[0].length = snprintf(buf, sizeof(buf), "AT+CMGS=%d\r", (int)tpdulen);
	at_cmd[0].data = ast_strdup(buf);
	if(!at_cmd[0].data)
	{
		ast_free(at_cmd[1].data);
		return -ENOMEM;
	}

	if (at_queue_insert_uid(cpvt, at_cmd, ITEMS_OF(at_cmd), 0, uid) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue a SMS message
 * \param cpvt -- cpvt structure
 * \param number -- the destination of the message
 * \param msg -- utf-8 encoded message
 */
int at_enqueue_sms(struct cpvt *cpvt, const char *destination, const char *msg, unsigned validity_minutes, int report_req, const char *payload, size_t payload_len)
{
	ssize_t res;
	pvt_t* pvt = cpvt->pvt;

	/* set default validity period */
	if (validity_minutes <= 0)
		validity_minutes = 3 * 24 * 60;

	int msg_len = strlen(msg);
	uint16_t msg_ucs2[msg_len * 2];
	res = utf8_to_ucs2(msg, msg_len, msg_ucs2, sizeof(msg_ucs2));
	if (res < 0) {
		chan_quectel_err = E_PARSE_UTF8;
		return -1;
	}

	char hexbuf[PDU_LENGTH * 2 + 1];

	pdu_part_t pdus[255];
	int csmsref = smsdb_get_refid(pvt->imsi, destination);
	if (csmsref < 0) {
		chan_quectel_err = E_SMSDB;
		return -1;
	}
	res = pdu_build_mult(pdus, "" /* pvt->sms_scenter */, destination, msg_ucs2, res, validity_minutes, !!report_req, csmsref);
	if (res < 0) {
		/* pdu_build_mult sets chan_quectel_err */
		return -1;
	}
	int uid = smsdb_outgoing_add(pvt->imsi, destination, res, validity_minutes * 60, report_req, payload, payload_len);
	if (uid < 0) {
		chan_quectel_err = E_SMSDB;
		return -1;
	}
	for (int i = 0; i < res; ++i) {
		hexify(pdus[i].buffer, pdus[i].length, hexbuf);
		if (at_enqueue_pdu(cpvt, hexbuf, pdus[i].length * 2, pdus[i].tpdu_length, uid) < 0) {
			return -1;
		}
	}

	return 0;
}

/*!
 * \brief Enqueue AT+CUSD.
 * \param cpvt -- cpvt structure
 * \param code the CUSD code to send
 */

int at_enqueue_ussd(struct cpvt *cpvt, const char *code)
{
	static const char cmd[] = "AT+CUSD=1,\"";
	static const char cmd_end[] = "\",15\r";
	at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_DYN(CMD_AT_CUSD);
	ssize_t res;
	int length;
	char buf[4096];

	memcpy (buf, cmd, STRLEN(cmd));
	length = STRLEN(cmd);
	int code_len = strlen(code);

	// use 7 bit encoding. 15 is 00001111 in binary and means 'Language using the GSM 7 bit default alphabet; Language unspecified' accodring to GSM 23.038
	uint16_t code16[code_len * 2];
	uint8_t code_packed[4069];
	res = utf8_to_ucs2(code, code_len, code16, sizeof(code16));
	if (res < 0) {
		chan_quectel_err = E_PARSE_UTF8;
		return -1;
	}
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

	memcpy(buf + length, cmd_end, STRLEN(cmd_end)+1);
	length += STRLEN(cmd_end);

	at_cmd.length = length;
	at_cmd.data = ast_strdup (buf);
	if (!at_cmd.data) {
		chan_quectel_err = E_UNKNOWN;
		return -1;
	}

	if (at_queue_insert(cpvt, &at_cmd, 1, 0) != 0) {
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

int at_enqueue_dtmf(struct cpvt *cpvt, char digit)
{
	switch (digit)
	{
/* unsupported, but AT^DTMF=1,22 OK and "2" sent
*/
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
			return -1974; // TODO: ???
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
			return at_enqueue_generic(cpvt, CMD_AT_DTMF, 1, "AT+VTS=%c\r", digit);
	}
	return -1;
}

/*!
 * \brief Enqueue the AT+CCWA command (disable call waiting)
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

int at_enqueue_set_ccwa(struct cpvt *cpvt, unsigned call_waiting)
{
	static const char cmd_ccwa_get[] = "AT+CCWA=1,2,1\r";
	static const char cmd_ccwa_set[] = "AT+CCWA=%d,%d,%d\r";
	int err;
	call_waiting_t value;
	at_queue_cmd_t cmds[] = {
		/* Set Call-Waiting On/Off */
		ATQ_CMD_DECLARE_DYNIT(CMD_AT_CCWA_SET, ATQ_CMD_TIMEOUT_MEDIUM, 0),
		/* Query CCWA Status for Voice Call  */
		ATQ_CMD_DECLARE_STIT(CMD_AT_CCWA_STATUS, cmd_ccwa_get, ATQ_CMD_TIMEOUT_MEDIUM, 0),
	};
	at_queue_cmd_t * pcmd = cmds;
	unsigned count = ITEMS_OF(cmds);

	if(call_waiting == CALL_WAITING_DISALLOWED || call_waiting == CALL_WAITING_ALLOWED)
	{
		value = call_waiting;
		err = call_waiting == CALL_WAITING_ALLOWED ? 1 : 0;
		err = at_fill_generic_cmd(&cmds[0], cmd_ccwa_set, err, err, CCWA_CLASS_VOICE);
		if (err) {
			chan_quectel_err = E_UNKNOWN;
		    return -1;
		}
	}
	else
	{
		value = CALL_WAITING_AUTO;
		pcmd++;
		count--;
	}
	CONF_SHARED(cpvt->pvt, callwaiting) = value;

	if (at_queue_insert(cpvt, pcmd, count, 0) != 0) {
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

int at_enqueue_reset(struct cpvt *cpvt)
{
	static const char cmd[] = "AT+CFUN=1,1\r";
	static const at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CFUN, cmd);

	if (at_queue_insert_const(cpvt, &at_cmd, 1, 0) != 0) {
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
int at_enqueue_dial(struct cpvt *cpvt, const char *number, int clir)
{
	struct pvt *pvt = cpvt->pvt;
	int err;
	int cmdsno = 0;
	char * tmp = NULL;
	at_queue_cmd_t cmds[6];


	if(PVT_STATE(pvt, chan_count[CALL_STATE_ACTIVE]) > 0 && CPVT_TEST_FLAG(cpvt, CALL_FLAG_HOLD_OTHER))
	{
		ATQ_CMD_INIT_ST(cmds[0], CMD_AT_CHLD_2, cmd_chld2);
/*  enable this cause response_clcc() see all calls are held and insert 'AT+CHLD=2'
		ATQ_CMD_INIT_ST(cmds[1], CMD_AT_CLCC, cmd_clcc);
*/
		cmdsno = 1;
	}

	if(clir != -1)
	{
		err = at_fill_generic_cmd(&cmds[cmdsno], "AT+CLIR=%d\r", clir);
		if (err) {
			chan_quectel_err = E_UNKNOWN;
			return -1;
		}
		tmp = cmds[cmdsno].data;
		ATQ_CMD_INIT_DYNI(cmds[cmdsno], CMD_AT_CLIR);
		cmdsno++;
	}
        if (pvt->is_simcom) {
	err = at_fill_generic_cmd(&cmds[cmdsno], "AT+CPCMREG=0;D%s;\r", number); }
        else if (CONF_UNIQ(pvt, uac)) {
        err = at_fill_generic_cmd(&cmds[cmdsno], "AT+QPCMV=0;+QPCMV=1,2;D%s;\r", number); }
        else {
        err = at_fill_generic_cmd(&cmds[cmdsno], "AT+QPCMV=0;+QPCMV=1,0;D%s;\r", number); }
	if(err)
	{
		ast_free(tmp);
		chan_quectel_err = E_UNKNOWN;
		return -1;
	}

	ATQ_CMD_INIT_DYNI(cmds[cmdsno], CMD_AT_D);
	cmdsno++;

/* on failed ATD this up held call */
	ATQ_CMD_INIT_ST(cmds[cmdsno], CMD_AT_CLCC, cmd_clcc);
	cmdsno++;

	if (at_queue_insert(cpvt, cmds, cmdsno, 1) != 0) {
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
int at_enqueue_answer(struct cpvt *cpvt)
{
	pvt_t* pvt = cpvt->pvt;
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_DYN(CMD_AT_A),
	};
	int count = ITEMS_OF(cmds);
	const char * cmd1;

	if(cpvt->state == CALL_STATE_INCOMING)
	{
/* FIXME: channel number? */
             if (pvt->is_simcom) {
		cmd1 = "AT+CPCMREG=0;A\r"; }
             else if (CONF_UNIQ(pvt, uac)) {
                cmd1 = "AT+QPCMV=0;+QPCMV=1,2;A\r"; }
             else { 
                cmd1 = "AT+QPCMV=0;+QPCMV=1,0;A\r"; }

	}
	else if(cpvt->state == CALL_STATE_WAITING)
	{
		cmds[0].cmd = CMD_AT_CHLD_2x;
		cmd1 = "AT+CHLD=2%d\r";
/* no need CMD_AT_DDSETEX in this case? */
		count--;
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Request answer for call idx %d with state '%s'\n", PVT_ID(cpvt->pvt), cpvt->call_idx, call_state2str(cpvt->state));
		return -1;
	}

	if (at_fill_generic_cmd(&cmds[0], cmd1, cpvt->call_idx) != 0) {
		chan_quectel_err = E_UNKNOWN;
		return -1;
	}
	if (at_queue_insert(cpvt, cmds, count, 1) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}
             if (pvt->is_simcom) {

                sleep(1);
                voice_enable(pvt);
                                  }
	return 0;
}

/*!
 * \brief Enqueue an activate commands 'Put active calls on hold and activate call x.'
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */
int at_enqueue_activate(struct cpvt *cpvt)
{
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_DYN(CMD_AT_CHLD_2x),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc),
		};

	if (cpvt->state == CALL_STATE_ACTIVE)
		return 0;

	if (cpvt->state != CALL_STATE_ONHOLD && cpvt->state != CALL_STATE_WAITING)
	{
		ast_log (LOG_ERROR, "[%s] Imposible activate call idx %d from state '%s'\n",
				PVT_ID(cpvt->pvt), cpvt->call_idx, call_state2str(cpvt->state));
		return -1;
	}

	if (at_fill_generic_cmd(&cmds[0], "AT+CHLD=2%d\r", cpvt->call_idx) != 0) {
		chan_quectel_err = E_UNKNOWN;
		return -1;
	}
	if (at_queue_insert(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
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
int at_enqueue_flip_hold(struct cpvt *cpvt)
{
	static const at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT_CHLD_2, cmd_chld2),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc),
		};

	if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue ping command
 * \param pvt -- pvt structure
 * \return 0 on success
 */
int at_enqueue_ping(struct cpvt *cpvt)
{
	static const at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_STIT(CMD_AT, cmd_at, ATQ_CMD_TIMEOUT_SHORT, 0),
		};

	if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Enqueue user-specified command
 * \param cpvt -- cpvt structure
 * \param input -- user's command
 * \return 0 on success
 */
int at_enqueue_user_cmd(struct cpvt *cpvt, const char *input)
{
	if (at_enqueue_generic(cpvt, CMD_USER, 1, "%s\r", input) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Start reading next SMS, if any
 * \param cpvt -- cpvt structure
 */
void at_retrieve_next_sms(struct cpvt *cpvt, at_cmd_suppress_error_t suppress_error)
{
	pvt_t *pvt = cpvt->pvt;
	unsigned int i;

	if (pvt->incoming_sms_index != -1U)
	{
		/* clear SMS index */
		i = pvt->incoming_sms_index;
		pvt->incoming_sms_index = -1U;

		/* clear this message index from inbox */
		sms_inbox_clear(pvt, i);
	}

	/* get next message to fetch from inbox */
	for (i = 0; i != SMS_INDEX_MAX; i++)
	{
		if (is_sms_inbox_set(pvt, i))
			break;
	}

	if (i == SMS_INDEX_MAX ||
	    at_enqueue_retrieve_sms(cpvt, i, suppress_error) != 0)
	{
		pvt_try_restate(pvt);
	}
}

/*!
 * \brief Enqueue commands for reading SMS
 * \param cpvt -- cpvt structure
 * \param index -- index of message in store
 * \return 0 on success
 */
int at_enqueue_retrieve_sms(struct cpvt *cpvt, int index, at_cmd_suppress_error_t suppress_error)
{
	pvt_t *pvt = cpvt->pvt;
	int err;
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_DYN2(CMD_AT_CMGR, RES_CMGR),
	};
	unsigned cmdsno = ITEMS_OF(cmds);

	if (suppress_error == SUPPRESS_ERROR_ENABLED) {
		cmds[0].flags |= ATQ_CMD_FLAG_SUPPRESS_ERROR;
	}

	/* set that we want to receive this message */
	if (!sms_inbox_set(pvt, index)) {
		chan_quectel_err = E_UNKNOWN;
		return -1;
	}

	/* check if message is already being received */
	if (pvt->incoming_sms_index != -1U) {
		ast_debug (4, "[%s] SMS retrieve of [%d] already in progress\n",
		    PVT_ID(pvt), pvt->incoming_sms_index);
		return 0;
	}

	pvt->incoming_sms_index = index;

	err = at_fill_generic_cmd (&cmds[0], "AT+CMGR=%d\r", index);
	if (err)
		goto error;

	err = at_queue_insert (cpvt, cmds, cmdsno, 0);
	if (err)
		goto error;
	return 0;
error:
	ast_log (LOG_WARNING, "[%s] SMS command error %d\n", PVT_ID(pvt), err);
	pvt->incoming_sms_index = -1U;
	chan_quectel_err = E_UNKNOWN;
	return -1;
}

/*!
 * \brief Enqueue commands for deleting SMS
 * \param cpvt -- cpvt structure
 * \param index -- index of message in store
 * \return 0 on success
 */
int at_enqueue_delete_sms(struct cpvt *cpvt, int index)
{
	int err;
	at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_DYN(CMD_AT_CMGD)
	};
	unsigned cmdsno = ITEMS_OF(cmds);

	err = at_fill_generic_cmd (&cmds[0], "AT+CMGD=%d\r", index);
	if (err) {
		chan_quectel_err = E_UNKNOWN;
		return err;
	}

	if (at_queue_insert(cpvt, cmds, cmdsno, 0) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}
	return 0;
}

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

		default: // use default one
		return AST_CAUSE_NORMAL_CLEARING;
	}
}

/*!
 * \brief Enqueue AT+CHLD1x or AT+CHUP hangup command
 * \param cpvt -- channel_pvt structure
 * \param call_idx -- call id
 * \return 0 on success
 */

int at_enqueue_hangup(struct cpvt *cpvt, int call_idx, int release_cause)
{
	struct pvt* pvt = cpvt->pvt;

	if (pvt->is_simcom) { // AT+CHUP
		static const char cmd_chup[] = "AT+CHUP\r";

		at_queue_cmd_t cmds[] = {
			ATQ_CMD_DECLARE_ST(CMD_AT_CHUP, cmd_chup),
			ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc),
			};

		if(cpvt == &pvt->sys_chan || cpvt->dir == CALL_DIR_INCOMING || (cpvt->state != CALL_STATE_INIT && cpvt->state != CALL_STATE_DIALING)) {
			/* FIXME: other channels may be in RELEASED or INIT state */
			if(PVT_STATE(pvt, chansno) > 1) {
				cmds[0].cmd = CMD_AT_CHLD_1x;
				const int err = at_fill_generic_cmd(&cmds[0], cmd_chld1x, call_idx);
				if (err) {
					chan_quectel_err = E_UNKNOWN;
					return -1;
				}
			}
		}

		if (at_queue_insert(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
			chan_quectel_err = E_QUEUE;
			return -1;
		}
	}
	else { // AT+QHUP=<cause>,<idx>
		at_queue_cmd_t cmds[] = {
			ATQ_CMD_DECLARE_DYN(CMD_AT_QHUP)
		};

		const int err = at_fill_generic_cmd(&cmds[0], "AT+QHUP=%d,%d\r", map_hangup_cause(release_cause), call_idx);
		if (err) {
			chan_quectel_err = E_UNKNOWN;
			return err;
		}

		if (at_queue_insert(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
			chan_quectel_err = E_QUEUE;
			return -1;
		}
	}
	return 0;
}

/*!
 * \brief Enqueue AT+CLVL commands for volume synchronization
 * \param cpvt -- cpvt structure
 * \return 0 on success
 */

int at_enqueue_volsync(struct cpvt *cpvt)
{
	static const char cmd1[] = "AT+CLVL=1\r";
	static const char cmd2[] = "AT+CLVL=5\r";
	static const at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT_CLVL, cmd1),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLVL, cmd2),
		};

	if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
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
int at_enqueue_clcc(struct cpvt *cpvt)
{
	static const at_queue_cmd_t at_cmd = ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc);

	if (at_queue_insert_const(cpvt, &at_cmd, 1, 1) != 0) {
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
int at_enqueue_conference(struct cpvt *cpvt)
{
	static const char cmd_chld3[] = "AT+CHLD=3\r";
	static const at_queue_cmd_t cmds[] = {
		ATQ_CMD_DECLARE_ST(CMD_AT_CHLD_3, cmd_chld3),
		ATQ_CMD_DECLARE_ST(CMD_AT_CLCC, cmd_clcc),
		};

	if (at_queue_insert_const(cpvt, cmds, ITEMS_OF(cmds), 1) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}
	return 0;
}


/*!
 * \brief SEND AT+CHUP command to device IMMEDIALITY
 * \param cpvt -- cpvt structure
 */
void at_hangup_immediality(struct cpvt* cpvt)
{
	char buf[20];
	int length = snprintf(buf, sizeof(buf), cmd_chld1x, cpvt->call_idx);

	if(length > 0)
		at_write(cpvt->pvt, buf, length);
}


int at_enqueue_mute(struct cpvt *cpvt, int mute)
{
	static const char cmd_cmut0[] = "AT+CMUT=0\r";
	static const char cmd_cmut1[] = "AT+CMUT=1\r";

	static const at_queue_cmd_t cmds0 = ATQ_CMD_DECLARE_ST(CMD_AT_CMUT_0, cmd_cmut0);
	static const at_queue_cmd_t cmds1 = ATQ_CMD_DECLARE_ST(CMD_AT_CMUT_1, cmd_cmut1);

	const at_queue_cmd_t* cmds = mute? &cmds1 : &cmds0;

	if (at_queue_insert_const(cpvt, cmds, 1, 1) != 0) {
		chan_quectel_err = E_QUEUE;
		return -1;
	}

	return 0;
}