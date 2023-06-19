/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>

   Copyright (C) 2020 Max von Buelow <max@m9x.de>
*/
#include "ast_config.h"

#include <asterisk/causes.h>		/* AST_CAUSE_... definitions */
#include <asterisk/logger.h>		/* ast_debug() */
#include <asterisk/pbx.h>			/* ast_pbx_start() */
#include <sys/sysinfo.h>
#include "ast_compat.h"				/* asterisk compatibility fixes */

#include "at_response.h"
#include "mutils.h"					/* STRLEN() */
#include "at_queue.h"
#include "chan_quectel.h"
#include "at_parse.h"
#include "char_conv.h"
#include "channel.h"				/* channel_queue_hangup() channel_queue_control() */
#include "smsdb.h"
#include "error.h"
#include "helpers.h"

#define CCWA_STATUS_NOT_ACTIVE	0
#define CCWA_STATUS_ACTIVE	1

#define CLCC_CALL_TYPE_VOICE	0
#define CLCC_CALL_TYPE_DATA	1
#define CLCC_CALL_TYPE_FAX	2

static const at_response_t at_responses_list[] = {

	AT_RESPONSES_TABLE(AT_RES_AS_STRUCTLIST)

	/* The hackish way to define the duplicated responses in the meantime */
#define DEF_STR(str)	str,STRLEN(str)
	{ RES_CNUM, "+CNUM",DEF_STR("ERROR+CNUM:") },
	{ RES_ERROR,"ERROR",DEF_STR("COMMAND NOT SUPPORT\r") },
#undef DEF_STR
	};


const at_responses_t at_responses = { at_responses_list, 2, ITEMS_OF(at_responses_list), RES_MIN, RES_MAX};

/*!
 * \brief Get the string representation of the given AT response
 * \param res -- the response to process
 * \return a string describing the given response
 */

const char* at_res2str (at_res_t res)
{
	if((int)res >= at_responses.name_first && (int)res <= at_responses.name_last)
		return at_responses.responses[res - at_responses.name_first].name;
	return "UNDEFINED";
}

static void request_clcc(struct pvt* pvt)
{
	if (at_enqueue_clcc(&pvt->sys_chan)) {
		ast_log(LOG_ERROR, "[%s] Error enqueue List Current Calls request\n", PVT_ID(pvt));
	}
}

#ifdef HANDLE_RCEND
static int at_response_rcend(struct pvt * pvt)
{
	int call_index = 0;
	unsigned int duration   = 0;
	int end_status = 0;
	int cc_cause   = 0;
	struct cpvt * cpvt;

	cpvt = active_cpvt(pvt);
	if (cpvt) {

		if(CPVT_IS_SOUND_SOURCE(cpvt)) at_enqueue_cpcmreg(&pvt->sys_chan, 0);
		call_index = cpvt->call_idx;
		ast_debug(1, "[%s] CEND: call_index %d duration %d end_status %d cc_cause %d Line disconnected\n"
			, PVT_ID(pvt), call_index, duration, end_status, cc_cause);
		CPVT_RESET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
		PVT_STAT(pvt, calls_duration[cpvt->dir]) += duration;
		change_channel_state(cpvt, CALL_STATE_RELEASED, cc_cause);
	}

	return 0;
}
#endif

#ifdef HANDLE_CEND
static int at_response_cend(struct pvt * pvt, const char* str)
{
	int call_index = 0;
	int duration   = 0;
	int end_status = 0;
	int cc_cause   = 0;
	struct cpvt * cpvt;
        
	request_clcc(pvt);

	/*
	 * parse CEND info in the following format:
	 * ^CEND:<call_index>,<duration>,<end_status>[,<cc_cause>]
	 */

	if (sscanf (str, "VOICE CALL: END: %d", &duration) != 1)
	{
		ast_debug (1, "[%s] Could not parse all CEND parameters\n", PVT_ID(pvt));
                return 0;
	}

	ast_debug (1, "[%s] CEND: call_index %d duration %d end_status %d cc_cause %d Line disconnected\n"
				, PVT_ID(pvt), call_index, duration, end_status, cc_cause);


	cpvt = active_cpvt(pvt);
	if (cpvt) {
		at_enqueue_cpcmreg(&pvt->sys_chan, 0);
		call_index = cpvt->call_idx;
		ast_debug (1,	"[%s] CEND: call_index %d duration %d end_status %d cc_cause %d Line disconnected\n"
			, PVT_ID(pvt), call_index, duration, end_status, cc_cause);
		CPVT_RESET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
		PVT_STAT(pvt, calls_duration[cpvt->dir]) += duration;
		change_channel_state(cpvt, CALL_STATE_RELEASED, cc_cause);
	}
	else {
		ast_log (LOG_ERROR, "[%s] CEND event for unknown call idx '%d'\n", PVT_ID(pvt), call_index);
	}
 
	return 0;
}
#endif

static int at_response_ok(struct pvt* pvt, at_res_t res)
{
	const at_queue_task_t* task = at_queue_head_task(pvt);
	const at_queue_cmd_t* ecmd = at_queue_task_cmd(task);

	if (!ecmd) {
		ast_log(LOG_ERROR, "[%s] Received unexpected 'OK'\n", PVT_ID(pvt));
		return 0;
	}

	if (ecmd->res == RES_OK) {
		switch (ecmd->cmd) {
			case CMD_AT:
			case CMD_AT_Z:
			case CMD_AT_E:
			case CMD_AT_CGMI:
			case CMD_AT_CGMM:
			case CMD_AT_CGMR:
			case CMD_AT_CMEE:
			case CMD_AT_CGSN:
			case CMD_AT_CIMI:
			case CMD_AT_CPIN:
			case CMD_AT_CCWA_SET:
			case CMD_AT_CCWA_STATUS:
			case CMD_AT_CHLD_2:
			case CMD_AT_CHLD_3:
			case CMD_AT_CSCA:
			case CMD_AT_CLCC:
			case CMD_AT_CLIR:
			case CMD_AT_QINDCFG_CSQ:
			case CMD_AT_QINDCFG_ACT:
			case CMD_AT_QINDCFG_RING:
			case CMD_AT_QINDCFG_CC:
			case CMD_AT_DSCI:
			case CMD_AT_QLTS:
			case CMD_AT_CCLK:
				ast_debug (4, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));
				break;

			case CMD_AT_FINAL:
				ast_verb(1, "[%s] Channel initialized\n", PVT_ID(pvt));
				if (CONF_UNIQ(pvt, uac) == TRIBOOL_NONE) {
					pvt_set_act(pvt, 1); // GSM
				}
				pvt->initialized = 1;
				break;

			case CMD_AT_COPS_INIT:
				ast_debug(1, "[%s] Operator select parameters set\n", PVT_ID(pvt));
				break;

			case CMD_AT_CREG_INIT:
			case CMD_AT_CEREG_INIT:
				ast_debug(1, "[%s] Registration info enabled\n", PVT_ID(pvt));
				break;

			case CMD_AT_CREG:
				ast_debug(1, "[%s] Registration query sent\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNUM:
				ast_debug(1, "[%s] Subscriber phone number query successed\n", PVT_ID(pvt));
				break;

			case CMD_AT_CVOICE:
				ast_debug(1, "[%s] Voice calls supported\n", PVT_ID(pvt));

				switch (CONF_UNIQ(pvt, uac)) {
					case TRIBOOL_TRUE:
					at_enqueue_enable_uac(&pvt->sys_chan);
					break;

					case TRIBOOL_FALSE:
					at_enqueue_enable_tty(&pvt->sys_chan);
					break;

					default:
					break;
				}
				break;

			case CMD_AT_CPCMREG:
				ast_debug(1, "[%s] Voice calls supported\n", PVT_ID(pvt));

				pvt->has_voice = 1;
				at_enqueue_cpcmfrm(task->cpvt, CONF_UNIQ(pvt, slin16));
				at_enqueue_cpcmreg(task->cpvt, 0);
				at_enqueue_cgains(task->cpvt, CONF_SHARED(pvt, txgain), CONF_SHARED(pvt, rxgain));
				at_enqueue_query_cgains(task->cpvt);
				break;

			case CMD_AT_QPCMV_0:
				ast_debug(4, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));

				pvt->has_voice = 0;
				break;
			
			case CMD_AT_QPCMV_TTY:
			case CMD_AT_QPCMV_UAC:
				ast_debug(4, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));

				pvt->has_voice = 1;
				at_enqueue_qgains(&pvt->sys_chan, CONF_SHARED(pvt, txgain), CONF_SHARED(pvt, rxgain));
				at_enqueue_query_qgains(&pvt->sys_chan);
				break;

/*
			case CMD_AT_CLIP:
				ast_debug(1, "[%s] Calling line indication disabled\n", PVT_ID(pvt));
				break;
*/
			case CMD_AT_CSSN:
				ast_debug(1, "[%s] Supplementary Service Notification enabled successful\n", PVT_ID(pvt));
				break;

			case CMD_AT_CMGF:
				ast_debug(1, "[%s] SMS operation mode set to PDU\n", PVT_ID(pvt));
				break;

			case CMD_AT_CSCS:
				ast_debug(1, "[%s] UCS-2 text encoding enabled\n", PVT_ID(pvt));

				pvt->use_ucs2_encoding = 1;
				break;

			case CMD_AT_CPMS:
				ast_debug(1, "[%s] SMS storage location is established\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNMI:
				ast_debug(1, "[%s] SMS supported\n", PVT_ID(pvt));
				ast_debug(1, "[%s] SMS new message indication mode enabled\n", PVT_ID(pvt));

				pvt->has_sms = 1;
				pvt->timeout = DATA_READ_TIMEOUT;
				break;

			case CMD_AT_D:
				pvt->dialing = 1;
				/* fall through */
			case CMD_AT_A:
			case CMD_AT_CHLD_2x:
/* not work, ^CONN: appear before OK for CHLD_ANSWER
				task->cpvt->answered = 1;
				task->cpvt->needhangup = 1;
*/
				CPVT_SET_FLAGS(task->cpvt, CALL_FLAG_NEED_HANGUP);
				ast_debug(4, "[%s] %s sent successfully for call id %d\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd), task->cpvt->call_idx);
				break;

			case CMD_AT_CFUN:
				/* in case of reset */
				pvt->ring = 0;
				pvt->dialing = 0;
				pvt->cwaiting = 0;
				break;

			case CMD_AT_CPCMREG1:
				ast_debug(4, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));
				if (!pvt->initialized) {
					pvt->timeout = DATA_READ_TIMEOUT;
					pvt->initialized = 1;
					ast_verb(3, "[%s] SimCom initialized and ready\n", PVT_ID(pvt));
				}
				break;

			case CMD_AT_CPCMREG0:
				ast_debug(4, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));
				break;

			case CMD_AT_CHUP:
			case CMD_AT_QHUP:
     		case CMD_AT_CHLD_1x:
				CPVT_RESET_FLAGS(task->cpvt, CALL_FLAG_NEED_HANGUP);
				ast_debug(1, "[%s] Successful hangup for call idx %d\n", PVT_ID(pvt), task->cpvt->call_idx);
				break;

			case CMD_AT_CMGS:
				ast_debug(1, "[%s] Sending sms message in progress\n", PVT_ID(pvt));
				break;

			case CMD_AT_SMSTEXT:
				pvt->outgoing_sms = 0;
				pvt_try_restate(pvt);

				/* TODO: move to +CMGS: handler */
				ast_verb(3, "[%s] Successfully sent SMS message %p\n", PVT_ID(pvt), task);
				break;

			case CMD_AT_DTMF:
				ast_debug(4, "[%s] DTMF sent successfully for call idx %d\n", PVT_ID(pvt), task->cpvt->call_idx);
				break;

			case CMD_AT_CUSD:
				ast_verb(3, "[%s] Successfully sent USSD %p\n", PVT_ID(pvt), task);
				break;

			case CMD_AT_COPS:
			case CMD_AT_QSPN:
				ast_debug (4, "[%s] Successfull provider query\n", PVT_ID(pvt));
				break;

			case CMD_AT_CMGR:
				ast_debug(3, "[%s] SMS message\n", PVT_ID(pvt));
				at_sms_retrieved(&pvt->sys_chan, 1);
				break;

			case CMD_AT_CMGD:
				ast_debug(4, "[%s] SMS message deleted successfully\n", PVT_ID(pvt));
				break;

			case CMD_AT_CSQ:
				ast_debug(1, "[%s] Got signal strength result\n", PVT_ID(pvt));
				break;

			case CMD_AT_AUTOCSQ_INIT:
			case CMD_AT_EXUNSOL_INIT:
				ast_debug(1, "[%s] Signal change notifications enabled\n", PVT_ID(pvt));
				break;

			case CMD_AT_CLTS_INIT:
				ast_debug(1, "[%s] Time update notifications enabled\n", PVT_ID(pvt));
				break;

			case CMD_AT_CLVL:
				pvt->volume_sync_step++;
				if(pvt->volume_sync_step == VOLUME_SYNC_DONE) {
					ast_debug(1, "[%s] Volume level synchronized\n", PVT_ID(pvt));
					pvt->volume_sync_step = VOLUME_SYNC_BEGIN;
				}
				break;

			case CMD_AT_CMUT_0:
				ast_debug(1, "[%s] Uplink voice unmuted\n", PVT_ID(pvt));
				break;

			case CMD_AT_CMUT_1:			
				ast_debug(1, "[%s] Uplink voice muted\n", PVT_ID(pvt));
				break;

			case CMD_AT_QTONEDET_0:
			case CMD_AT_DDET_0:
				ast_debug(1, "[%s] Tone detection disabled\n", PVT_ID(pvt));
				break;

			case CMD_AT_QTONEDET_1:
			case CMD_AT_DDET_1:
				ast_debug(1, "[%s] Tone detection enabled\n", PVT_ID(pvt));
				break;

			case CMD_AT_QMIC:
			case CMD_AT_QRXGAIN:
			case CMD_AT_CMICGAIN:
			case CMD_AT_COUTGAIN:
				ast_debug(1, "[%s] TX/RX gains updated\n", PVT_ID(pvt));
				break;

			case CMD_AT_CRXVOL:
			case CMD_AT_CTXVOL:
				ast_debug(3, "[%s] TX/RX volume updated\n", PVT_ID(pvt));
				break;

			case CMD_AT_CMGL:
				ast_debug(1, "[%s] Messages listed\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNMA:
				ast_debug(1, "[%s] Message confirmed\n", PVT_ID(pvt));
				break;

			case CMD_AT_CSMS:
				ast_debug(1, "[%s] Message service channel configured\n", PVT_ID(pvt));
				break;

			case CMD_AT_QAUDLOOP:
				ast_debug(1, "[%s] Audio loop configured\n", PVT_ID(pvt));
				break;

			case CMD_AT_QAUDMOD:
				ast_debug(1, "[%s] Audio mode configured\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNSMOD_0:
				ast_debug(1, "[%s] Network mode notifications disabled\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNSMOD_1:
				ast_debug(1, "[%s] Network mode notifications enabled\n", PVT_ID(pvt));
				break;

			case CMD_AT_CPCMFRM_8K:
				ast_log(LOG_NOTICE, "[%s] Audio sample rate set to 8kHz\n", PVT_ID(pvt));
				break;

			case CMD_AT_CPCMFRM_16K:
				ast_log(LOG_NOTICE, "[%s] Audio sample rate set to 16kHz\n", PVT_ID(pvt));
				break;

			case CMD_AT_VTD:
				ast_debug(2, "[%s] Tone duration updated\n", PVT_ID(pvt));
				break;

			case CMD_AT_CCID:
				ast_debug(3, "[%s] ICCID obtained\n", PVT_ID(pvt));
				break;

			case CMD_AT_CICCID:
			case CMD_AT_QCCID:
				ast_debug(3, "[%s] ICCID obtained\n", PVT_ID(pvt));
				break;

			case CMD_USER:
				break;

			default:
				ast_log(LOG_ERROR, "[%s] Received 'OK' for unhandled command '%s'\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));
				break;
		}
		at_queue_handle_result(pvt, res);
	}
	else {
		ast_log(LOG_ERROR, "[%s] Received 'OK' when expecting '%s', ignoring\n", PVT_ID(pvt), at_res2str(ecmd->res));
	}

	return 0;
}

static void log_cmd_response_error(const struct pvt* pvt, const at_queue_cmd_t *ecmd, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ast_log_ap(LOG_ERROR, fmt, ap);
	va_end(ap);
}

/*!
 * \brief Handle ERROR response
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_error(struct pvt* pvt, at_res_t res)
{
	const at_queue_task_t* task = at_queue_head_task(pvt);
	const at_queue_cmd_t* ecmd = at_queue_task_cmd(task);

	if (ecmd && (ecmd->res == RES_OK || ecmd->res == RES_SMS_PROMPT)) {
		switch (ecmd->cmd)
		{
			/* critical errors */
			case CMD_AT:
			case CMD_AT_Z:
			case CMD_AT_E:
			case CMD_AT_CLCC:
				log_cmd_response_error(pvt, ecmd, "[%s] Command '%s' failed\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));
				/* mean disconnected from device */
				goto e_return;

			case CMD_AT_FINAL:
				log_cmd_response_error(pvt, ecmd, "[%s] Channel not initialized\n", PVT_ID(pvt));
				pvt->initialized = 0;
				goto e_return;

			/* not critical errors */
			case CMD_AT_CCWA_SET:
			case CMD_AT_CCWA_STATUS:
			case CMD_AT_CNUM:
				log_cmd_response_error(pvt, ecmd, "[%s] Command '%s' failed\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));
				/* mean ignore error */
				break;

			case CMD_AT_CGMI:
				log_cmd_response_error(pvt, ecmd, "[%s] Getting manufacturer info failed\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CGMM:
				log_cmd_response_error(pvt, ecmd, "[%s] Getting model info failed\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CGMR:
				log_cmd_response_error(pvt, ecmd, "[%s] Getting firmware info failed\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CMEE:
				log_cmd_response_error(pvt, ecmd, "[%s] Setting error verbosity level failed\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CGSN:
				log_cmd_response_error(pvt, ecmd, "[%s] Getting IMEI number failed\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CIMI:
				log_cmd_response_error(pvt, ecmd, "[%s] Getting IMSI number failed\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CPIN:
				log_cmd_response_error(pvt, ecmd, "[%s] Error checking PIN state\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_COPS_INIT:
				log_cmd_response_error(pvt, ecmd, "[%s] Error setting operator select parameters\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CREG_INIT:
				log_cmd_response_error(pvt, ecmd, "[%s] Error enabling registration info\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CEREG_INIT:
				ast_debug(1, "[%s] Error enabling registration info\n", PVT_ID(pvt));
				break;

			case CMD_AT_AUTOCSQ_INIT:
			case CMD_AT_EXUNSOL_INIT:
				ast_debug(1, "[%s] Error enabling CSQ(E) report\n", PVT_ID(pvt));
				break;

			case CMD_AT_CLTS_INIT:
				ast_debug(2, "[%s] Time update notifications not available\n", PVT_ID(pvt));
				break;

			case CMD_AT_CREG:
				ast_debug(1, "[%s] Error getting registration info\n", PVT_ID(pvt));
				break;

			case CMD_AT_QINDCFG_CSQ:
			case CMD_AT_QINDCFG_ACT:
			case CMD_AT_QINDCFG_RING:
			case CMD_AT_QINDCFG_CC:
			case CMD_AT_DSCI:
				ast_debug(1, "[%s] Error enabling indications\n", PVT_ID(pvt));
				break;

			case CMD_AT_CVOICE:
				ast_debug (1, "[%s] Voice calls not supported\n", PVT_ID(pvt));
				pvt->has_voice = 0;
                break;

			case CMD_AT_CPCMREG:
				if (CONF_UNIQ(pvt, uac) > TRIBOOL_FALSE) {
					pvt->has_voice = 1;
				}
				else {
					ast_debug(1, "[%s] Voice calls not supported\n", PVT_ID(pvt));
					pvt->has_voice = 0;
				}
				break;
/*
			case CMD_AT_CLIP:
				log_cmd_response_error(pvt, ecmd, "[%s] Error enabling calling line indication\n", PVT_ID(pvt));
				goto e_return;
*/
			case CMD_AT_CSSN:
				log_cmd_response_error(pvt, ecmd, "[%s] Error Supplementary Service Notification activation failed\n", PVT_ID(pvt));
				goto e_return;

			case CMD_AT_CMGF:
			case CMD_AT_CPMS:
			case CMD_AT_CNMI:
				ast_debug (1, "[%s] No SMS support\n", PVT_ID(pvt));
				ast_debug (1, "[%s] Command '%s' failed\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));

				pvt->has_sms = 0;
				pvt->timeout = DATA_READ_TIMEOUT;
				break;

			case CMD_AT_CSCS:
				ast_debug(1, "[%s] No UCS-2 encoding support\n", PVT_ID(pvt));

				pvt->use_ucs2_encoding = 0;
				break;

			case CMD_AT_A:
			case CMD_AT_CHLD_2x:
				log_cmd_response_error(pvt, ecmd, "[%s] Answer failed for call idx %d\n", PVT_ID(pvt), task->cpvt->call_idx);
				queue_hangup (task->cpvt->channel, AST_CAUSE_CALL_REJECTED);
				break;

			case CMD_AT_CHLD_3:
				log_cmd_response_error(pvt, ecmd, "[%s] Can't begin conference call idx %d\n", PVT_ID(pvt), task->cpvt->call_idx);
				queue_hangup(task->cpvt->channel, AST_CAUSE_CALL_REJECTED);
				break;

			case CMD_AT_CLIR:
				log_cmd_response_error(pvt, ecmd, "[%s] Setting CLIR failed\n", PVT_ID(pvt));
				break;

			case CMD_AT_CHLD_2:
				if (!CPVT_TEST_FLAG(task->cpvt, CALL_FLAG_HOLD_OTHER) || task->cpvt->state != CALL_STATE_INIT) {
					break;
				}
				/* fall through */
			case CMD_AT_D:
				log_cmd_response_error(pvt, ecmd, "[%s] Dial failed\n", PVT_ID(pvt));
				queue_control_channel (task->cpvt, AST_CONTROL_CONGESTION);
				break;

			case CMD_AT_CPCMREG1:
				if (CONF_UNIQ(pvt, uac) == TRIBOOL_FALSE && CPVT_IS_SOUND_SOURCE(task->cpvt)) {
					ast_debug(3, "[%s] Trying to activate audio stream again\n", PVT_ID(pvt));
					at_enqueue_cpcmreg(task->cpvt, 1);
				}
				else {
					log_cmd_response_error(pvt, ecmd, "[%s] Could not activate audio stream\n", PVT_ID(pvt));
				}
				break;

			case CMD_AT_CPCMREG0:
				log_cmd_response_error(pvt, ecmd, "[%s] Could not deactivate audio stream\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));
				break;

			case CMD_AT_CHUP:
			case CMD_AT_QHUP:
			case CMD_AT_CHLD_1x:
				log_cmd_response_error(pvt, ecmd, "[%s] Error sending hangup for call idx %d\n", PVT_ID(pvt), task->cpvt->call_idx);
				break;

			case CMD_AT_CMGR:
				// log_cmd_response_error(pvt, ecmd, "[%s] Error reading SMS message, resetting index\n", PVT_ID(pvt));
				at_sms_retrieved(&pvt->sys_chan, 0);
				break;

			case CMD_AT_CMGD:
				log_cmd_response_error(pvt, ecmd, "[%s] Error deleting SMS message\n", PVT_ID(pvt));
				break;

			case CMD_AT_CMGS:
			case CMD_AT_SMSTEXT:
				pvt->outgoing_sms = 0;
				pvt_try_restate(pvt);

				{
					char payload[SMSDB_PAYLOAD_MAX_LEN];
					char dst[SMSDB_DST_MAX_LEN];
					ssize_t payload_len = smsdb_outgoing_clear(task->uid, dst, payload);
					if (payload_len >= 0) {
						ast_verb(3, "[%s] Error payload: %.*s\n", PVT_ID(pvt), (int) payload_len, payload);
						start_local_report_channel(pvt, dst, payload, NULL, NULL, 0, 'i', NULL);
					}
				}

				ast_verb(3, "[%s] Error sending SMS message %p\n", PVT_ID(pvt), task);
				log_cmd_response_error(pvt, ecmd, "[%s] Error sending SMS message %p %s\n", PVT_ID(pvt), task, at_cmd2str (ecmd->cmd));
				break;

			case CMD_AT_DTMF:
				log_cmd_response_error(pvt, ecmd, "[%s] Error sending DTMF\n", PVT_ID(pvt));
				break;

			case CMD_AT_COPS:
			case CMD_AT_QSPN:
				ast_debug(1, "[%s] Could not get provider name\n", PVT_ID(pvt));
				break;

			case CMD_AT_QLTS:
			case CMD_AT_CCLK:
				ast_debug(1, "[%s] Could not query time\n", PVT_ID(pvt));
				break;

			case CMD_AT_CLVL:
				ast_debug(1, "[%s] Audio level synchronization failed at step %d/%d\n", PVT_ID(pvt), pvt->volume_sync_step, VOLUME_SYNC_DONE-1);
				pvt->volume_sync_step = VOLUME_SYNC_BEGIN;
				break;

			case CMD_AT_CUSD:
				ast_verb(3, "[%s] Error sending USSD %p\n", PVT_ID(pvt), task);
				log_cmd_response_error(pvt, ecmd, "[%s] Error sending USSD %p\n", PVT_ID(pvt), task);
				break;

			case CMD_AT_CMUT_0:
				ast_debug(1, "[%s] Cannot unmute uplink voice\n", PVT_ID(pvt));
				break;
			
			case CMD_AT_CMUT_1:			
				ast_debug(1, "[%s] Cannot mute uplink voice\n", PVT_ID(pvt));
				break;

			case CMD_AT_QPCMV_0:
				ast_log(LOG_WARNING, "[%s] Cannot disable UAC\n", PVT_ID(pvt));
				break;

			case CMD_AT_QPCMV_TTY:
				ast_log(LOG_WARNING, "[%s] Cannot enable audio on serial port\n", PVT_ID(pvt));
				break;

			case CMD_AT_QPCMV_UAC:
				ast_log(LOG_WARNING, "[%s] Cannot enable UAC\n", PVT_ID(pvt));
				break;

			case CMD_AT_QTONEDET_0:
			case CMD_AT_DDET_0:
				ast_log(LOG_WARNING, "[%s] Cannot disable tone detection\n", PVT_ID(pvt));
				break;

			case CMD_AT_QTONEDET_1:
			case CMD_AT_DDET_1:
				ast_log(LOG_WARNING, "[%s] Cannot enable tone detection\n", PVT_ID(pvt));
				break;

			case CMD_AT_QMIC:
			case CMD_AT_QRXGAIN:
			case CMD_AT_COUTGAIN:
			case CMD_AT_CMICGAIN:
				ast_log(LOG_WARNING, "[%s] Cannot update TX/RG gain\n", PVT_ID(pvt));
				break;

			case CMD_AT_CTXVOL:
			case CMD_AT_CRXVOL:
				ast_log(LOG_WARNING, "[%s] Cannot update TX/RG volume\n", PVT_ID(pvt));
				break;

			case CMD_AT_CMGL:
				ast_debug(1, "[%s] Cannot list messages\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNMA:
				ast_log(LOG_WARNING, "[%s] Cannot confirm message reception\n", PVT_ID(pvt));
				break;

			case CMD_AT_CSMS:
				ast_log(LOG_WARNING, "[%s] Message service channel not configured\n", PVT_ID(pvt));
				break;

			case CMD_AT_QAUDLOOP:
				ast_log(LOG_WARNING, "[%s] Audio loop not configured\n", PVT_ID(pvt));
				break;

			case CMD_AT_QAUDMOD:
				ast_log(LOG_WARNING, "[%s] Audio mode not configured\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNSMOD_0:
				ast_log(LOG_WARNING, "[%s] Could not disable network mode notifications\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNSMOD_1:
				ast_log(LOG_WARNING, "[%s] Could not enable network mode notifications\n", PVT_ID(pvt));
				break;

			case CMD_AT_CPCMFRM_8K:
			case CMD_AT_CPCMFRM_16K:
				ast_log(LOG_WARNING, "[%s] Could not set audio sample rate\n", PVT_ID(pvt));
				break;

			case CMD_AT_VTD:
				ast_log(LOG_WARNING, "[%s] Could not set tone duration\n", PVT_ID(pvt));
				break;

			case CMD_AT_CCID:
				ast_log(LOG_WARNING, "[%s] Could not get ICCID\n", PVT_ID(pvt));
				break;

			case CMD_AT_CICCID:
			case CMD_AT_QCCID:
				ast_log(LOG_WARNING, "[%s] Could not get ICCID\n", PVT_ID(pvt));
				break;

			case CMD_USER:
				break;

			default:
				log_cmd_response_error(pvt, ecmd, "[%s] Received 'ERROR' for unhandled command '%s'\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));
				break;
		}
		at_queue_handle_result(pvt, res);
	}
	else if (ecmd) {
		log_cmd_response_error(pvt, ecmd, "[%s] Received 'ERROR' when expecting '%s', ignoring\n", PVT_ID(pvt), at_res2str (ecmd->res));
	}
	else {
		log_cmd_response_error(pvt, ecmd, "[%s] Received unexpected 'ERROR'\n", PVT_ID(pvt));
	}

	return 0;

e_return:
	at_queue_handle_result(pvt, res);
	return -1;
}

static int start_pbx(struct pvt* pvt, const char * number, int call_idx, call_state_t state)
{
	struct cpvt* cpvt;

	/* TODO: pass also Subscriber number or other DID info for exten  */
#if ASTERISK_VERSION_NUM >= 120000 /* 12+ */
	struct ast_channel * channel = new_channel(
			pvt, AST_STATE_RING, number, call_idx, CALL_DIR_INCOMING, state,
			pvt->has_subscriber_number ? pvt->subscriber_number : CONF_SHARED(pvt, exten),
			NULL, NULL, 0);
#else /* 12- */
	struct ast_channel * channel = new_channel(
			pvt, AST_STATE_RING, number, call_idx, CALL_DIR_INCOMING, state,
			pvt->has_subscriber_number ? pvt->subscriber_number : CONF_SHARED(pvt, exten),
			NULL, 0);
#endif /* ^12- */

	if (!channel) {
		ast_log(LOG_ERROR, "[%s] Unable to allocate channel for incoming call\n", PVT_ID(pvt));

		if (at_enqueue_hangup(&pvt->sys_chan, call_idx, AST_CAUSE_DESTINATION_OUT_OF_ORDER)) {
			ast_log(LOG_ERROR, "[%s] Error sending AT+CHUP command\n", PVT_ID(pvt));
		}

		return -1;
	}

	cpvt = ast_channel_tech_pvt(channel);
	// FIXME: not execute if channel_new() failed
	CPVT_SET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);

	/* ast_pbx_start() usually failed if asterisk.conf minmemfree
	 * set too low, try drop buffer cache
	 * sync && echo 3 >/proc/sys/vm/drop_caches
	 */
	if (ast_pbx_start(channel)) {
		ast_channel_tech_pvt_set(channel, NULL);
		cpvt_free(cpvt);

		ast_hangup (channel);
		ast_log (LOG_ERROR, "[%s] Unable to start pbx on incoming call\n", PVT_ID(pvt));
		// TODO: count fails and reset incoming when count reach limit ?
		return -1;
	}

	return 0;
}

static void handle_clcc(struct pvt* pvt,
	unsigned call_idx, unsigned dir,
	unsigned state, unsigned mode,
	unsigned mpty,
	const char* number,
	unsigned type)
{
	struct cpvt* cpvt = pvt_find_cpvt(pvt, (int)call_idx);

	if(cpvt) {
		/* cpvt alive */
		CPVT_SET_FLAGS(cpvt, CALL_FLAG_ALIVE);

		if(dir != cpvt->dir) {
			ast_log(LOG_ERROR, "[%s] CLCC call idx:%d - direction mismatch %d/%d\n", PVT_ID(pvt), cpvt->call_idx, dir, cpvt->dir);
			return;
		}

		if (CONF_SHARED(pvt, multiparty)) {
			if(mpty)
				CPVT_SET_FLAGS(cpvt, CALL_FLAG_MULTIPARTY);
			else
				CPVT_RESET_FLAGS(cpvt, CALL_FLAG_MULTIPARTY);
		}
		else {
			if (!CPVT_TEST_FLAG(cpvt, CALL_FLAG_MULTIPARTY) && mpty) {
				ast_log(LOG_ERROR, "[%s] Rejecting multiparty call - idx:%d\n", PVT_ID(pvt), call_idx);
				at_enqueue_hangup(&pvt->sys_chan, call_idx, AST_CAUSE_CALL_REJECTED);
			}
		}

		if(state != cpvt->state) {
			change_channel_state(cpvt, state, 0);
		}
		else {
			return;
		}
	}
	else {
		switch (state)
		{
			case CALL_STATE_DIALING:
			case CALL_STATE_ALERTING:
			cpvt = last_initialized_cpvt(pvt);
			if (!CONF_SHARED(pvt, multiparty)) {
				if (!CPVT_TEST_FLAG(cpvt, CALL_FLAG_MULTIPARTY) && mpty) {
					cpvt = NULL;
				}
			}

			if (cpvt) {
				cpvt->call_idx = (short)call_idx;
				change_channel_state(cpvt, state, 0);				
			} else {
				at_enqueue_hangup(&pvt->sys_chan, call_idx, AST_CAUSE_CALL_REJECTED);
				ast_log(LOG_ERROR, "[%s] Answered unexisting or multiparty incoming call - idx:%d, hanging up!\n", PVT_ID(pvt), call_idx);
				return;
			}
			break;
		}
	}

	if (cpvt || state == CALL_STATE_INCOMING) {
		ast_debug(3, "[%s] CLCC idx:%u dir:%u state:%u mode:%u mpty:%u number:%s type:%u\n", PVT_ID(pvt), call_idx, dir, state, mode, mpty, number, type);		
	}
	else {
		ast_log(LOG_WARNING, "[%s] CLCC (not found) idx:%u dir:%u state:%u mode:%u mpty:%u number:%s type:%u\n", PVT_ID(pvt), call_idx, dir, state, mode, mpty, number, type);			
	}

	switch(state) {
		case CALL_STATE_ACTIVE:
			if (cpvt && pvt->is_simcom && CONF_UNIQ(pvt, uac) == TRIBOOL_FALSE && pvt->has_voice) {
				at_enqueue_cpcmreg(cpvt, 1);
			}
			break;

		case CALL_STATE_DIALING:
		{
			pvt->dialing = 1;
			pvt->cwaiting = 0;
			pvt->ring = 0;
			break;
		}

		case CALL_STATE_ALERTING:
		{
			pvt->dialing = 1;
			pvt->cwaiting = 0;
			pvt->ring = 0;

			PVT_STAT(pvt, calls_answered[cpvt->dir]) ++;
			if(CPVT_TEST_FLAG(cpvt, CALL_FLAG_CONFERENCE)) at_enqueue_conference(cpvt);
			break;
		}

		case CALL_STATE_INCOMING:
		{
			pvt->ring = 1;
			pvt->dialing = 0;
			pvt->cwaiting = 0;

			PVT_STAT(pvt, in_calls) ++;

			if (pvt_enabled(pvt)) {
				/* TODO: give dialplan level user tool for checking device is voice enabled or not  */
				if(start_pbx(pvt, number, call_idx, state) == 0) {
					PVT_STAT(pvt, in_calls_handled) ++;
					if(!pvt->has_voice)
						ast_log(LOG_WARNING, "[%s] pbx started for device not voice capable\n", PVT_ID(pvt));
				}
				else {
					PVT_STAT(pvt, in_pbx_fails) ++;
				}
			}
			break;
		}

		case CALL_STATE_WAITING:
		{
			pvt->cwaiting = 1;
			pvt->ring = 0;
			pvt->dialing = 0;

			PVT_STAT(pvt, cw_calls) ++;
	
			if (dir == CALL_DIR_INCOMING) {
				if (pvt_enabled(pvt)) {
					/* TODO: give dialplan level user tool for checking device is voice enabled or not  */
					if(start_pbx(pvt, number, call_idx, state) == 0) {
						PVT_STAT(pvt, in_calls_handled) ++;
						if(!pvt->has_voice)
							ast_log(LOG_WARNING, "[%s] pbx started for device not voice capable\n", PVT_ID(pvt));
					}
					else {
						PVT_STAT(pvt, in_pbx_fails) ++;
					}
				}
			}
			break;
		}
		
		case CALL_STATE_RELEASED: // CALL END
		{
			pvt->ring = 0;
			pvt->dialing = 0;
			pvt->cwaiting = 0;

			if (cpvt && pvt->is_simcom && CONF_UNIQ(pvt, uac) == TRIBOOL_FALSE && pvt->has_voice) {
				at_enqueue_cpcmreg(cpvt, 0);
			}
			break;
		}

		default:
		ast_log(LOG_WARNING, "[%s] Unhandled call state event - idx:%u call_state:%s-%u)\n", PVT_ID(pvt), call_idx, call_state2str((call_state_t)state), state);
		break;
	}
}

/*!
 * \brief Handle +CLCC response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_clcc(struct pvt* pvt, char* str)
{
	struct cpvt * cpvt;
	unsigned call_idx, dir, state, mode, mpty, type;
	char * number;
	char *p;

	if (!pvt->initialized) {
		return 0;
	}

	AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry)
	{
		CPVT_RESET_FLAGS(cpvt, CALL_FLAG_ALIVE);
	}

	for(;;) {
		p = strchr(str, '\r');
		if(at_parse_clcc(str, &call_idx, &dir, &state, &mode, &mpty, &number, &type) == 0) {
			int process_clcc = 1;
			if (mode != CLCC_CALL_TYPE_VOICE) {
				ast_debug(4, "[%s] CLCC - non-voice call, idx:%u dir:%u state:%u nubmer:%s\n", PVT_ID(pvt), call_idx, dir, state, number);
				process_clcc = 0;
			}

			if (mode > CALL_STATE_WAITING) {
				ast_log(LOG_ERROR, "[%s] CLCC - wrong call state, line '%s'\n", PVT_ID(pvt), str);
				process_clcc = 0;
			}

			if (process_clcc) {
				handle_clcc(pvt, call_idx, dir, state, mode, mpty, number, type);
			}
		}
		else {
			ast_log(LOG_ERROR, "[%s] CLCC - can't parse line '%s'\n", PVT_ID(pvt), str);
		}

		if(p) { // next line
			++p;
			if(p[0] == '\n') ++p;
			if(p[0]) {
				str = p;
				continue;
			}
		}

		break;
	}

	return 0;
}

#ifdef HANDLE_DSCI
/*!
 * \brief Handle ^DSCI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \retval  0 success
 * \retval -1 error
 */
static int at_response_dsci(struct pvt* pvt, char* str)
{
	unsigned int call_index;
	unsigned int call_dir;
	unsigned int call_state;
	unsigned int call_type;
	char* number;
	unsigned int number_type;

	if (at_parse_dsci(str, &call_index, &call_dir, &call_state, &call_type, &number, &number_type)) {
		ast_log(LOG_ERROR, "[%s] Fail to parse DSCI '%s'\n", PVT_ID(pvt), str);
		return 0;
	}

	if (call_type != CLCC_CALL_TYPE_VOICE) {
		ast_debug(4, "[%s] Non-voice DSCI - idx:%u dir:%d type:%u state:%u number:%s\n", PVT_ID(pvt), call_index, call_dir, call_type, call_state, number);
		return 0;
	}

	ast_debug(3, "[%s] DSCI - idx:%u dir:%u type:%u state:%u number:%s\n", PVT_ID(pvt), call_index, call_dir, call_type, call_state, number);

	switch (call_state) {
		case CALL_STATE_RELEASED: // released call will not be listed by AT+CLCC command, handle directly
		handle_clcc(pvt, call_index, call_dir, call_state, call_type, 2u, number, number_type);
		break;

		default: // request CLCC anyway
		request_clcc(pvt);
		break;
	}
	
	return 0;
}
#endif

/*!
 * \brief Handle +QIND response.
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_qind(struct pvt* pvt, char* str)
{
	int res;

	qind_t qind;
	char* params;

	res = at_parse_qind(str, &qind, &params);
	if (res < 0) {
		return -1;
	}

	ast_debug(4, "[%s] QIND(%s) - %s\n", PVT_ID(pvt), at_qind2str(qind), params);

	switch(qind) {
		case QIND_CSQ:
		{
			int rssi;
			char buf[40];

			res = at_parse_qind_csq(params, &rssi);
			if (res < 0) {
				ast_debug(3, "[%s] Failed to parse CSQ - %s\n", PVT_ID(pvt), params);
				break;
			}
			ast_verb(3, "[%s] RSSI: %s\n", PVT_ID(pvt), rssi2dBm(rssi, buf, sizeof(buf)));
			pvt->rssi = rssi;
			return 0;
		}

		case QIND_ACT:
		{
			int act;
			res = at_parse_qind_act(params, &act);
			if (res < 0) {
				ast_debug(3, "[%s] Failed to parse ACT - %s\n", PVT_ID(pvt), params);
				break;
			}
			ast_verb(1, "[%s] Access technology: %s\n", PVT_ID(pvt), sys_act2str(act));
			pvt_set_act(pvt, act);

			if (act) {
				if (pvt->is_simcom) {
					if (at_enqueue_cops(&pvt->sys_chan)) {
						ast_log(LOG_WARNING, "[%s] Error sending query for provider name\n", PVT_ID(pvt));
					}
				}
				else {
					if (at_enqueue_qspn_qnwinfo(&pvt->sys_chan)) {
						ast_log(LOG_WARNING, "[%s] Error sending query for provider name\n", PVT_ID(pvt));
					}
				}
			}
			return 0;
		}

		case QIND_CCINFO:
		{
			unsigned call_idx, dir, state, mode, mpty, toa;
			char* number;

			res = at_parse_qind_cc(params, &call_idx, &dir, &state, &mode, &mpty, &number, &toa);
			if (res < 0) {
				ast_log(LOG_ERROR, "[%s] Fail to parse CCINFO - %s\n", PVT_ID(pvt), params);
				break;
			}
			handle_clcc(pvt, call_idx, dir, state, mode, mpty, number, toa);
			return 0;
		}

		case QIND_NONE:
		return 0;
	}

	return -1;
}

/*!
 * \brief Handle +CSCA response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */
static int at_response_csca(struct pvt* pvt, char* str)
{
	char*   csca;
	if(at_parse_csca(str, &csca)) {
		ast_debug(1, "[%s] Could not parse CSCA response '%s'\n", PVT_ID(pvt), str);
		return -1;
	}

	if (pvt->use_ucs2_encoding) {
		char csca_utf8_str[20];
		const int csca_nibbles = unhex(csca, (uint8_t*)csca);
		const ssize_t res = ucs2_to_utf8((const uint16_t*)csca, (csca_nibbles + 1) / 4, csca_utf8_str, STRLEN(csca_utf8_str));
		if (res < 0) {
			return -1;
		}
		csca_utf8_str[res] = '\000';
		ast_string_field_set(pvt, sms_scenter, csca_utf8_str);
	} else { // ASCII
		ast_string_field_set(pvt, sms_scenter, csca);
	}

	ast_debug(2, "[%s] CSCA: %s\n", PVT_ID(pvt), pvt->sms_scenter);
	return 0;
}

/*!
 * \brief Handle +CCWA response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_ccwa(struct pvt* pvt, char* str)
{
	int status, n;
	unsigned class;

	/*
	 * CCWA may be in form:
	 *	in response of AT+CCWA=?
	 *		+CCWA: (0,1)
	 *	in response of AT+CCWA=?
	 *		+CCWA: <n>
	 *	in response of "AT+CCWA=[<n>[,<mode>[,<class>]]]"
	 *		+CCWA: <status>,<class1>
	 *	unsolicited result code
	 *		+CCWA: <number>,<type>,<class>,[<alpha>][,<CLI validity>[,<subaddr>,<satype>[,<priority>]]]
	 *
	 */
	if (sscanf(str, "+CCWA: (%u-%u)", &status, &class) == 2)
		return 0;

	n = sscanf (str, "+CCWA:%d,%d", &status, &class);
	if(n == 1) return 0;
	else if (n == 2) {
		if ((class & CCWA_CLASS_VOICE) && (status == CCWA_STATUS_NOT_ACTIVE || status == CCWA_STATUS_ACTIVE)) {
			pvt->has_call_waiting = status == CCWA_STATUS_ACTIVE ? 1 : 0;
			ast_verb(1, "[%s] Call waiting is %s\n", PVT_ID(pvt), status ? "enabled" : "disabled");
		}
		return 0;
	}

	if (pvt->initialized) {
		if (at_parse_ccwa(str, &class) == 0) {
			if (class == CCWA_CLASS_VOICE) {
				pvt->cwaiting = 1;
				request_clcc(pvt);
			}
		}
		else
			ast_log (LOG_ERROR, "[%s] Can't parse CCWA line '%s'\n", PVT_ID(pvt), str);
	}
	
	return 0;
}

/*!
 * \brief Poll for SMS messages
 * \param pvt -- pvt structure
 * \retval 0 success
 * \retval -1 failure
 */
int
at_poll_sms(struct pvt *pvt)
{
	if (CONF_SHARED(pvt, disablesms)) {
		return -1;
	}

	return at_enqueue_list_messages(&pvt->sys_chan, MSG_STAT_REC_UNREAD);
}

/*!
 * \brief Handle +CMTI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cmti(struct pvt* pvt, const char* str)
{
// FIXME: check format in PDU mode
	int idx = at_parse_cmti(str);

	if (CONF_SHARED(pvt, disablesms)) {
		ast_log (LOG_WARNING, "[%s] SMS reception has been disabled in the configuration.\n", PVT_ID(pvt));
		return 0;
	}

	if (idx > -1) {
		ast_debug(1, "[%s] Incoming SMS message - IDX:%d\n", PVT_ID(pvt), idx);

		if (at_enqueue_retrieve_sms(&pvt->sys_chan, idx)) {
			ast_log(LOG_ERROR, "[%s] Error sending CMGR to retrieve SMS message\n", PVT_ID(pvt));
			return -1;
		}
	}
	else {
		ast_log(LOG_WARNING, "[%s] Error parsing incoming SMS message alert '%s' (ignoring)\n", PVT_ID(pvt), str);
	}

	return 0;
}

/*!
 * \brief Handle +CDSI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cdsi (struct pvt* pvt, const char* str)
{
// FIXME: check format in PDU mode
	int index = at_parse_cdsi(str);

	if (CONF_SHARED(pvt, disablesms)) {
		ast_log(LOG_WARNING, "[%s] SMS reception has been disabled in the configuration.\n", PVT_ID(pvt));
		return 0;
	}

	if (index > -1) {
		ast_debug(1, "[%s] Incoming SMS message\n", PVT_ID(pvt));

		if (at_enqueue_retrieve_sms(&pvt->sys_chan, index)) {
			ast_log(LOG_ERROR, "[%s] Error sending CMGR to retrieve SMS message\n", PVT_ID(pvt));
			return -1;
		}
	}
	else {
		/* Not sure why this happens, but we don't want to disconnect standing calls.
		 * [Jun 14 19:57:57] ERROR[3056]: at_response.c:1173 at_response_cmti:
		 *   [m1-1] Error parsing incoming sms message alert '+CMTI: "SM",-1' */
		ast_log(LOG_WARNING, "[%s] Error parsing incoming sms message alert '%s', ignoring\n", PVT_ID(pvt), str);
	}

	return 0;
}

/*!
 * \brief Handle +CMGR response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_msg(struct pvt* pvt, const struct ast_str* const response, at_res_t cmd)
{
	static const size_t MAX_MSG_LEN = 4096;

	char scts[64], dt[64];
	int mr, st;
	int res;
	int tpdu_type, idx;
	pdu_udh_t udh;
	tristate_bool_t msg_ack = TRIBOOL_NONE;
	int msg_complete = 0;

	pdu_udh_init(&udh);

	scts[0] = '\000';
	struct ast_str* msg = ast_str_create(MAX_MSG_LEN);
	struct ast_str* oa = ast_str_create(512);
	struct ast_str* sca = ast_str_create(512);
	size_t msg_len = ast_str_size(msg);

	switch (cmd) {
		case RES_CMGR:
		res = at_parse_cmgr(ast_str_buffer(response), ast_str_strlen(response), &tpdu_type, ast_str_buffer(sca), ast_str_size(sca), ast_str_buffer(oa), ast_str_size(oa), scts, &mr, &st, dt, ast_str_buffer(msg), &msg_len, &udh);
		break;

		case RES_CMGL:
		res = at_parse_cmgl(ast_str_buffer(response), ast_str_strlen(response), &idx, &tpdu_type, ast_str_buffer(sca), ast_str_size(sca), ast_str_buffer(oa), ast_str_size(oa), scts, &mr, &st, dt, ast_str_buffer(msg), &msg_len, &udh);
		break;

		case RES_CMT:
		res = at_parse_cmt(ast_str_buffer(response), ast_str_strlen(response), &tpdu_type, ast_str_buffer(sca), ast_str_size(sca), ast_str_buffer(oa), ast_str_size(oa), scts, &mr, &st, dt, ast_str_buffer(msg), &msg_len, &udh);
		break;

		case RES_CBM:
		res = at_parse_cbm(ast_str_buffer(response), ast_str_strlen(response), &tpdu_type, ast_str_buffer(sca), ast_str_size(sca), ast_str_buffer(oa), ast_str_size(oa), scts, &mr, &st, dt, ast_str_buffer(msg), &msg_len, &udh);
		break;

		case RES_CDS:
		res = at_parse_cds(ast_str_buffer(response), ast_str_strlen(response), &tpdu_type, ast_str_buffer(sca), ast_str_size(sca), ast_str_buffer(oa), ast_str_size(oa), scts, &mr, &st, dt, ast_str_buffer(msg), &msg_len, &udh);
		break;

		default:
		res = -1;
		break;
	}

	if (res < 0) {
		ast_str_reset(msg);
		ast_str_reset(oa);
		ast_str_reset(sca);
		ast_free(msg);
		ast_free(oa);
		ast_free(sca);
		ast_log(LOG_WARNING, "[%s] Error parsing incoming message: %s\n", PVT_ID(pvt), error2str(chan_quectel_err));
		msg_ack = TRIBOOL_FALSE;
		goto msg_done_ack;
	}

	ast_str_update(msg);
	switch (PDUTYPE_MTI(tpdu_type)) {
		case PDUTYPE_MTI_SMS_STATUS_REPORT: {
			static const size_t STATUS_REPORT_STR_LEN = 255 * 4 + 1;
			ast_verb(1, "[%s] Got status report with ref %d from %s and status code %d\n", PVT_ID(pvt), mr, ast_str_buffer(oa), st);

			int* const status_report = (int*)ast_malloc(sizeof(int)*256);
			struct ast_str* status_report_str = ast_str_create(STATUS_REPORT_STR_LEN);
			struct ast_str* payload = ast_str_create(SMSDB_PAYLOAD_MAX_LEN);
			const ssize_t payload_len = smsdb_outgoing_part_status(pvt->imsi, ast_str_buffer(oa), mr, st, status_report, ast_str_buffer(payload));
			if (payload_len >= 0) {
				int success = 1;
				int srroff = 0;
				for (int i = 0; status_report[i] != -1; ++i) {
					success &= !(status_report[i] & 0x40);
					ast_str_append(&status_report_str, STATUS_REPORT_STR_LEN, "%03d,", status_report[i]);
					srroff += 4;
				}
				*(ast_str_buffer(payload) + payload_len) = '\0';
				ast_str_update(payload);
				ast_verb(2, "[%s] Success: %d; Payload: %.*s; Report string: %s\n", PVT_ID(pvt), success, (int)payload_len, ast_str_buffer(payload), ast_str_buffer(status_report_str));
				msg_ack = TRIBOOL_TRUE;
				msg_complete = 1;
				start_local_report_channel(pvt, ast_str_buffer(oa), ast_str_buffer(payload), scts, dt, success, 'e', ast_str_buffer(status_report_str));
			}

			ast_free(status_report);
			ast_free(status_report_str);
			ast_free(payload);
			break;
		}

		case PDUTYPE_MTI_SMS_DELIVER: {
			struct ast_str* fullmsg = ast_str_create(MAX_MSG_LEN);
			if (udh.parts > 1) {
				ast_verb(2, "[%s] Got SM part from %s: '%s'; [ref=%d, parts=%d, order=%d]\n", PVT_ID(pvt), ast_str_buffer(oa), ast_str_buffer(msg), udh.ref, udh.parts, udh.order);
				int csms_cnt = smsdb_put(pvt->imsi, ast_str_buffer(oa), udh.ref, udh.parts, udh.order, ast_str_buffer(msg), ast_str_buffer(fullmsg));
				if (csms_cnt <= 0) {
					ast_log(LOG_ERROR, "[%s] Error putting SMS to SMSDB\n", PVT_ID(pvt));
					goto receive_as_is;
				}
				ast_str_update(fullmsg);

				if (pvt->is_simcom) {
					msg_ack = TRIBOOL_TRUE;
				}
				else {
					msg_ack = (udh.order == 1)? TRIBOOL_TRUE : TRIBOOL_NONE;
				}

				if (udh.order < udh.parts) {
					msg_complete = 0;
					goto msg_done;
				}
				else {
					msg_complete = 1;
					if (csms_cnt < (int)udh.parts) {
						ast_log(LOG_WARNING, "[%s] Incomplete SMS, got %d of %d parts\n", PVT_ID(pvt), csms_cnt, (int)udh.parts);
					}
				}

			} else {
receive_as_is:
				msg_ack = TRIBOOL_TRUE;
				msg_complete = 1;
				ast_verb(2, "[%s] Got single SM from %s: [%s]\n", PVT_ID(pvt), ast_str_buffer(oa), ast_str_buffer(msg));
				ast_str_copy_string(&fullmsg, msg);
			}

			ast_verb(1, "[%s] Got SMS from %s: [TS:%s][%s]\n", PVT_ID(pvt), ast_str_buffer(oa), scts, ast_str_buffer(fullmsg));

			struct ast_str* b64 = ast_str_create(40800);
			ast_base64encode(ast_str_buffer(b64), (unsigned char*)ast_str_buffer(fullmsg), ast_str_strlen(fullmsg), ast_str_size(b64));
			ast_str_update(b64);

			const channel_var_t vars[] =
			{
				{ "SMS", ast_str_buffer(fullmsg) } ,
				{ "SMS_BASE64", ast_str_buffer(b64) },
				{ "SMS_TS", scts },
				{ NULL, NULL },
			};
			start_local_channel(pvt, "sms", ast_str_buffer(oa), vars);

			ast_free(b64);
			ast_free(fullmsg);
			break;
		}
	}

msg_done:

	ast_free(msg);
	ast_free(oa);
	ast_free(sca);

	if (CONF_SHARED(pvt, autodeletesms) && msg_complete) {
		switch(cmd) {
			case RES_CMGR:
			at_enqueue_delete_sms(&pvt->sys_chan, pvt->incoming_sms_index, TRIBOOL_NONE);
			break;

			case RES_CMGL:
			at_enqueue_delete_sms(&pvt->sys_chan, idx, (CONF_SHARED(pvt, msg_service) > 0)? msg_ack : TRIBOOL_NONE);
			goto msg_ret;

			default:
			break;
		}
	}

msg_done_ack:
	switch(cmd) {
		case RES_CMGR:
		at_sms_retrieved(&pvt->sys_chan, 0);
		break;

		case RES_CMT:
		case RES_CDS:
		case RES_CMGL:
		if (CONF_SHARED(pvt, msg_service) > 0) {
			switch(msg_ack) {
				case TRIBOOL_FALSE: // negative ACT
				at_enqueue_msg_ack(&pvt->sys_chan);
				// at_enqueue_msg_ack_n(&pvt->sys_chan, 2);
				break;

				case TRIBOOL_TRUE: // positive ACK
				at_enqueue_msg_ack(&pvt->sys_chan);
				// at_enqueue_msg_ack_n(&pvt->sys_chan, 1);
				break;

				default:
				break;
			}
		}
		break;

		default:
		break;
	}

msg_ret:

	return 0;
}

/*!
 * \brief Send an SMS message from the queue.
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_sms_prompt (struct pvt* pvt)
{
	const struct at_queue_cmd * ecmd = at_queue_head_cmd (pvt);
	if (ecmd && ecmd->res == RES_SMS_PROMPT)
	{
		at_queue_handle_result (pvt, RES_SMS_PROMPT);
	}
	else if (ecmd)
	{
		ast_log (LOG_ERROR, "[%s] Received sms prompt when expecting '%s' response to '%s', ignoring\n", PVT_ID(pvt),
				at_res2str (ecmd->res), at_cmd2str (ecmd->cmd));
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Received unexpected sms prompt\n", PVT_ID(pvt));
	}

	return 0;
}

/*!
 * \brief Handle CUSD response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cusd(struct pvt* pvt, const struct ast_str* const result, int gsm7)
{
	static const char * const types[] = {
		"USSD Notify",
		"USSD Request",
		"USSD Terminated by network",
		"Other local client has responded",
		"Operation not supported",
		"Network time out",
	};

	ssize_t	res;
	int	type;
	char* cusd;
	int	dcs;
	char cusd_utf8_str[1024];
	char text_base64[16384];
	char typebuf[2];
	const char*	typestr;

	if (at_parse_cusd(ast_str_buffer(result), &type, &cusd, &dcs)) {
		ast_verb(1, "[%s] Error parsing CUSD: [%s]\n", PVT_ID(pvt), ast_str_buffer(result));
		return -1;
	}

	if (type < 0 || type >= (int)ITEMS_OF(types)) {
		ast_log(LOG_WARNING, "[%s] Unknown CUSD type: %d\n", PVT_ID(pvt), type);
	}

	typestr = enum2str(type, types, ITEMS_OF(types));

	typebuf[0] = type + '0';
	typebuf[1] = 0;

	if (dcs >= 0) {
		// sanitize DCS
		if (!gsm7) {
			dcs = 2;
		} else if (dcs & 0x40) {
			dcs = (dcs & 0xc) >> 2;
			if (dcs == 3) dcs = 0;
		} else {
			dcs = 0;
		}

		if (dcs == 0) { // GSM-7
			uint16_t out_ucs2[1024];
			const int cusd_nibbles = unhex(cusd, (uint8_t*)cusd);
			res = gsm7_unpack_decode(cusd, cusd_nibbles, out_ucs2, sizeof(out_ucs2) / 2, 0, 0, 0);
			if (res < 0) {
				return -1;
			}
			res = ucs2_to_utf8(out_ucs2, res, cusd_utf8_str, STRLEN(cusd_utf8_str));
		} else if (dcs == 1) { // ASCII
			res = strlen(cusd);
			if ((size_t)res > STRLEN(cusd_utf8_str)) {
				res = -1;
			} else {
				memcpy(cusd_utf8_str, cusd, res);
			}
		} else if (dcs == 2) { // UCS-2
			const int cusd_nibbles = unhex(cusd, (uint8_t*)cusd);
			res = ucs2_to_utf8((const uint16_t*)cusd, (cusd_nibbles + 1) / 4, cusd_utf8_str, STRLEN(cusd_utf8_str));
		} else {
			res = -1;
		}

		if (res < 0) {
			return -1;
		}
		cusd_utf8_str[res] = '\000';

		ast_verb(1, "[%s] Got USSD type %d '%s': [%s]\n", PVT_ID(pvt), type, typestr, cusd_utf8_str);
		ast_base64encode(text_base64, (unsigned char*)cusd_utf8_str, res, sizeof(text_base64));
	}
	else {
		text_base64[0] = '\000';
		ast_verb(1, "[%s] Got USSD type %d '%s'\n", PVT_ID(pvt), type, typestr);
	}

	{
		const channel_var_t vars[] =
		{
			{ "USSD_TYPE", typebuf },
			{ "USSD_TYPE_STR", ast_strdupa(typestr) },
			{ "USSD", cusd_utf8_str },
			{ "USSD_BASE64", text_base64 },
			{ NULL, NULL },
		};
		start_local_channel(pvt, "ussd", "ussd", vars);
	}
	return 0;
}

/*!
 * \brief Handle +CPIN response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cpin (struct pvt* pvt, char* str, size_t len)
{
	int rv = at_parse_cpin (str, len);
	switch(rv)
	{
		case -1:
			ast_log (LOG_ERROR, "[%s] Error parsing +CPIN message: %s\n", PVT_ID(pvt), str);
			break;
		case 1:
			ast_log (LOG_ERROR, "[%s] needs PIN code!\n", PVT_ID(pvt));
			break;
		case 2:
			ast_log (LOG_ERROR, "[%s] needs PUK code!\n", PVT_ID(pvt));
			break;
	}
	return rv;
}

/*!
 * \brief Handle ^SMMEMFULL response This event notifies us, that the sms storage is full
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_smmemfull (struct pvt* pvt)
{
	ast_log (LOG_ERROR, "[%s] SMS storage is full\n", PVT_ID(pvt));
	return 0;
}

static int at_response_csq(struct pvt* pvt, const struct ast_str* const response)
{
	int rssi;

	if(at_parse_csq(ast_str_buffer(response), &rssi)) {
		ast_debug(2, "[%s] Error parsing +CSQ result '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	char buf[40];
	ast_verb(3, "[%s] RSSI: %s\n", PVT_ID(pvt), rssi2dBm(rssi, buf, sizeof(buf)));
	pvt->rssi = rssi;
	return 0;
}

static int at_response_csqn(struct pvt* pvt, const struct ast_str* const response)
{
	int rssi, ber;
	if (at_parse_csqn(ast_str_buffer(response), &rssi, &ber)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	char buf[40];
	ast_verb(3, "[%s] RSSI: %s\n", PVT_ID(pvt), rssi2dBm(rssi, buf, sizeof(buf)));
	ast_verb(4, "[%s] BER: %d\n", PVT_ID(pvt), ber);
	pvt->rssi = rssi;
	return 0;
}

/*!
 * \brief Handle +CNUM response Here we get our own phone number
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cnum (struct pvt* pvt, char* str)
{
	char* number = at_parse_cnum (str);

	if (number)
	{
		ast_string_field_set(pvt, subscriber_number, number);
		if(pvt->subscriber_number[0] != 0)
			pvt->has_subscriber_number = 1;
		return 0;
	}

	ast_string_field_set(pvt, subscriber_number, "Unknown");
	pvt->has_subscriber_number = 0;

	return -1;
}

/*!
 * \brief Handle +COPS response Here we get the GSM provider name
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cops(struct pvt* pvt, char* str)
{
	char* provider_name = at_parse_cops(str);

	if (!provider_name) {
		ast_string_field_set(pvt, provider_name, "NONE");
		ast_verb(1, "[%s] Provider name: %s\n", PVT_ID(pvt), pvt->provider_name);
		return -1;
	}


	ast_string_field_set(pvt, provider_name, provider_name);
	ast_verb(1, "[%s] Provider name: %s\n", PVT_ID(pvt), pvt->provider_name);
	return 0;
}

static int at_response_qspn(struct pvt* pvt, char* str)
{
	char* fnn; // full network name
	char* snn; // short network name
	char* spn; // service provider name

	if (at_parse_qspn(str, &fnn, &snn, &spn)) {
		ast_log(LOG_ERROR, "[%s] Error parsing QSPN response - '%s'", PVT_ID(pvt), str);
		return -1;
	}

	ast_verb(1, "[%s] Operator: %s/%s/%s\n", PVT_ID(pvt), fnn, snn, spn);

	ast_string_field_set(pvt, network_name, fnn);
	ast_string_field_set(pvt, short_network_name, snn);
	ast_string_field_set(pvt, provider_name, spn);
	return 0;
}

static int at_response_qnwinfo(struct pvt* pvt, char* str)
{
	int act; // access technology
	int oper; // operator in numeric format
	char* band; // selected band
	int channel; // channel ID

	if (at_parse_qnwinfo(str, &act, &oper, &band, &channel)) {
		ast_log(LOG_WARNING, "[%s] Error parsing QNWINFO response - '%s'", PVT_ID(pvt), str);
		return -1;
	}

	if (act < 0) return 0;

	pvt_set_act(pvt, act);
	pvt->operator = oper;
	ast_string_field_set(pvt, band, band);

	ast_verb(1, "[%s] Registered PLMN: %d\n", PVT_ID(pvt), oper);
	ast_verb(1, "[%s] Band: %s\n", PVT_ID(pvt), band);
	return 0;
}

/*!
 * \brief Handle +CREG response Here we get the GSM registration status
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_creg(struct pvt* pvt, char* str, size_t len)
{
	int	gsm_reg;
	char* lac;
	char* ci;
	int act;

	if (at_parse_creg(str, len, &gsm_reg, &pvt->gsm_reg_status, &lac, &ci, &act)) {
		ast_log(LOG_ERROR, "[%s] Error parsing CREG: '%.*s'\n", PVT_ID(pvt), (int)len, str);
		return 0;
	}

	if (gsm_reg) {
		if (pvt->is_simcom) {
			if (at_enqueue_cops(&pvt->sys_chan)) {
				ast_log(LOG_WARNING, "[%s] Error sending query for provider name\n", PVT_ID(pvt));
			}
		}
		else {
			if (at_enqueue_qspn_qnwinfo(&pvt->sys_chan)) {
				ast_log(LOG_WARNING, "[%s] Error sending query for provider name\n", PVT_ID(pvt));
			}
		}

//#ifdef ISSUE_CCWA_STATUS_CHECK
		/* only if gsm_registered 0 -> 1 ? */
		if(!pvt->gsm_registered && CONF_SHARED(pvt, callwaiting) != CALL_WAITING_AUTO) {
			if (at_enqueue_set_ccwa(&pvt->sys_chan, CONF_SHARED(pvt, callwaiting))) {
				ast_log(LOG_WARNING, "[%s] Error setting call waiting mode\n", PVT_ID(pvt));
			}
		}
//#endif
		pvt->gsm_registered = 1;
		ast_string_field_set(pvt, location_area_code, lac);
		ast_string_field_set(pvt, cell_id, ci);

		ast_verb(1, "[%s] Location area code: %s\n", PVT_ID(pvt), S_OR(lac, ""));
		ast_verb(1, "[%s] Cell ID: %s\n", PVT_ID(pvt), S_OR(ci, ""));
	}
	else {
		pvt->gsm_registered = 0;
		ast_string_field_set(pvt, location_area_code, NULL);
		ast_string_field_set(pvt, cell_id, NULL);
	}

	return 0;
}

/*!
 * \brief Handle AT+CGMI response
 * \param pvt -- pvt structure
 * \param response -- string containing response
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgmi(struct pvt* pvt, const struct ast_str* const response)
{
	static const char MANUFACTURER_QUECTEL[] = "Quectel";
	static const char MANUFACTURER_SIMCOM[] = "SimCom";

	ast_string_field_set(pvt, manufacturer, ast_str_buffer(response));

	if (!strncasecmp(ast_str_buffer(response), MANUFACTURER_QUECTEL, STRLEN(MANUFACTURER_QUECTEL))) {
		ast_verb(1, "[%s] Quectel module\n", PVT_ID(pvt));
		pvt->is_simcom = 0;
		pvt->has_voice = 0;
		return at_enqueue_initialization_quectel(&pvt->sys_chan);
	}
	else if (!strncasecmp(ast_str_buffer(response), MANUFACTURER_SIMCOM, STRLEN(MANUFACTURER_SIMCOM))) {
		ast_verb(1, "[%s] SimCOM module\n", PVT_ID(pvt));
		pvt->is_simcom = 1;
		pvt->has_voice = 0;
		return at_enqueue_initialization_simcom(&pvt->sys_chan);
	}
	else {
		ast_log(LOG_WARNING, "[%s] Unknown module manufacturer: %s", PVT_ID(pvt), ast_str_buffer(response));
		pvt->has_voice = 0;
		return at_enqueue_initialization_other(&pvt->sys_chan);
	}

	return 0;
}

/*!
 * \brief Handle AT+CGMM response
 * \param pvt -- pvt structure
 * \param response -- string containing response
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

#/* */
static int at_response_cgmm(struct pvt* pvt, const struct ast_str* const response)
{
	ast_string_field_set(pvt, model, ast_str_buffer(response));

	return 0;
}

/*!
 * \brief Handle AT+CGMR response
 * \param pvt -- pvt structure
 * \param response -- string containing response
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgmr(struct pvt* pvt, const struct ast_str* const response)
{
	ast_string_field_set(pvt, firmware, ast_str_buffer(response));

	return 0;
}

/*!
 * \brief Handle AT+CGSN response
 * \param pvt -- pvt structure
 * \param response -- string containing response
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgsn(struct pvt* pvt, const struct ast_str* const response)
{
	ast_string_field_set(pvt, imei, ast_str_buffer(response));

	return 0;
}

/*!
 * \brief Handle AT+CIMI response
 * \param pvt -- pvt structure
 * \param response -- string containing response
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cimi(struct pvt* pvt, const struct ast_str* const response)
{
	ast_string_field_set(pvt, imsi, ast_str_buffer(response));

	return 0;
}

static int at_response_ccid(struct pvt* pvt, const struct ast_str* const response)
{
	ast_string_field_set(pvt, iccid, ast_str_buffer(response));

	return 0;
}

static int at_response_xccid(struct pvt* pvt, const struct ast_str* const response)
{
	char* ccid;
	if (at_parse_xccid(ast_str_buffer(response), &ccid)) {
		ast_log(LOG_ERROR, "[%s] Error parsing CCID: '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return 0;
	}

	ast_string_field_set(pvt, iccid, ccid);
	return 0;
}

static void at_response_busy(struct pvt* pvt, enum ast_control_frame_type control)
{
	const struct at_queue_task * task = at_queue_head_task (pvt);
	struct cpvt* cpvt = task->cpvt;

	if(cpvt)
	{
		CPVT_SET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
		queue_control_channel (cpvt, control);
	}
}

static void send_dtmf_frame(struct pvt* const pvt, char c)
{
	if (!CONF_SHARED(pvt, dtmf)) {
		ast_debug(1, "[%s] Detected DTMF: %c", PVT_ID(pvt), c);
		return;
	}

	struct cpvt* const cpvt = active_cpvt(pvt);
	if (cpvt && cpvt->channel) {
		struct ast_frame f = { AST_FRAME_DTMF, };
		f.len = CONF_SHARED(pvt, dtmf_duration);
		f.subclass.integer = c;
		if (ast_queue_frame(cpvt->channel, &f)) {
			ast_log(LOG_ERROR, "[%s] Fail to send detected DTMF: %c", PVT_ID(pvt), c);
		}
		else {
			ast_verb(1, "[%s] Detected DTMF: %c", PVT_ID(pvt), c);
		}
	}
	else {
		ast_log(LOG_WARNING, "[%s] Detected DTMF: %c", PVT_ID(pvt), c);
	}
}

static void at_response_qtonedet(struct pvt* pvt, const struct ast_str* const result)
{
	int dtmf;
	char c = '\000';

	if (at_parse_qtonedet(ast_str_buffer(result), &dtmf)) {
		ast_log(LOG_ERROR, "[%s] Error parsing QTONEDET: '%s'\n", PVT_ID(pvt), ast_str_buffer(result));
		return;
	}

	switch(dtmf) {
		case 48:
			c = '0';
			break;

		case 49:
			c = '1';
			break;

		case 50:
			c = '2';
			break;

		case 51:
			c = '3';
			break;

		case 52:
			c = '4';
			break;

		case 53:
			c = '5';
			break;

		case 54:
			c = '6';
			break;

		case 55:
			c = '7';
			break;

		case 56:
			c = '8';
			break;

		case 57:
			c = '9';
			break;

		case 65:
			c = 'A';
			break;

		case 66:
			c = 'B';
			break;

		case 67:
			c = 'C';
			break;

		case 68:
			c = 'D';
			break;

		case 42:
			c = '*';
			break;

		case 35:
			c = '#';
			break;
	}

	if (!c) {
		ast_log(LOG_WARNING, "[%s] Detected unknown DTMF code: %d", PVT_ID(pvt), dtmf);
		return;
	}

	send_dtmf_frame(pvt, c);
}

static void at_response_dtmf(struct pvt* pvt, const struct ast_str* const result)
{
	char c = '\000';
	if (at_parse_dtmf(ast_str_buffer(result), &c)) {
		ast_log(LOG_ERROR, "[%s] Error parsing RXDTMF: '%s'\n", PVT_ID(pvt), ast_str_buffer(result));
		return;
	}
	send_dtmf_frame(pvt, c);
}

static const char* qpcmv2str(int qpcmv)
{
	const char* const names[3] = {
		"USB NMEA port",
		"Debug UART",
		"USB sound card"
	};
	return enum2str_def((unsigned)qpcmv, names, ITEMS_OF(names), "Unknown");
}

static void at_response_qpcmv(struct pvt* pvt, char* str, size_t len)
{
	int enabled;
	int mode;

	if (at_parse_qpcmv(str, &enabled, &mode)) {
		ast_log(LOG_ERROR, "[%s] Error parsing QPCMV: '%.*s'\n", PVT_ID(pvt), (int)len, str);
		return;
	}

	ast_debug(1, "[%s] Voice configuration: %s [%s]\n", PVT_ID(pvt), qpcmv2str(mode), S_COR(enabled, "enabled", "disabled"));
}

static void at_response_qlts(struct pvt* pvt, const struct ast_str* const result)
{
	char* ts;

	if (at_parse_qlts(ast_str_buffer(result), &ts)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(result));
		return;
	}

	ast_verb(3, "[%s] Module time: %s\n", PVT_ID(pvt), ts);
	ast_string_field_set(pvt, module_time, ts);	
}

static void at_response_cclk(struct pvt* pvt, char* str, size_t len)
{
	char* ts;

	if (at_parse_cclk(str, &ts)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%.*s'\n", PVT_ID(pvt), (int)len, str);
		return;
	}

	ast_verb(3, "[%s] Module time: %s\n", PVT_ID(pvt), ts);
	ast_string_field_set(pvt, module_time, ts);
}

static void at_response_qrxgain(struct pvt* pvt, const struct ast_str* const response)
{
	int gain;

	if (at_parse_qrxgain(ast_str_buffer(response), &gain)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return;
	}

	struct ast_str* const sgain = gain2str(gain);
	ast_verb(1, "[%s] RX Gain: %s [%d]\n", PVT_ID(pvt), ast_str_buffer(sgain), gain);
	ast_free(sgain);
}

static void at_response_qmic(struct pvt* pvt, const struct ast_str* const response)
{
	int gain, dgain;

	if (at_parse_qmic(ast_str_buffer(response), &gain, &dgain)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return;
	}

	struct ast_str* const sgain = gain2str(gain);
	ast_verb(1, "[%s] Microphone Gain: %s [%d], %d\n", PVT_ID(pvt), ast_str_buffer(sgain), gain, dgain);
	ast_free(sgain);
}

static void at_response_cmicgain(struct pvt* pvt, const struct ast_str* const response)
{
	int gain;

	if (at_parse_cxxxgain(ast_str_buffer(response), &gain)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return;
	}

	struct ast_str* const sgain = gain2str_simcom(gain);
	ast_verb(1, "[%s] RX Gain: %s [%d]\n", PVT_ID(pvt), ast_str_buffer(sgain), gain);
	ast_free(sgain);
}

static void at_response_coutgain(struct pvt* pvt, const struct ast_str* const response)
{
	int gain;

	if (at_parse_cxxxgain(ast_str_buffer(response), &gain)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return;
	}

	struct ast_str* const sgain = gain2str_simcom(gain);
	ast_verb(1, "[%s] TX Gain: %s [%d]\n", PVT_ID(pvt), ast_str_buffer(sgain), gain);
	ast_free(sgain);
}

static void at_response_crxvol(struct pvt* pvt, const struct ast_str* const response)
{
	int gain;

	if (at_parse_cxxvol(ast_str_buffer(response), &gain)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return;
	}

	struct ast_str* const sgain = gain2str(gain);
	ast_verb(1, "[%s] RX Volume: %s [%d]\n", PVT_ID(pvt), ast_str_buffer(sgain), gain);
	ast_free(sgain);
}

static void at_response_ctxvol(struct pvt* pvt, const struct ast_str* const response)
{
	int gain;

	if (at_parse_cxxvol(ast_str_buffer(response), &gain)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return;
	}

	struct ast_str* const sgain = gain2str(gain);
	ast_verb(1, "[%s] Microphone Volume: %s [%d]\n", PVT_ID(pvt), ast_str_buffer(sgain), gain);
	ast_free(sgain);
}

static int at_response_csms(struct pvt*, const struct ast_str* const)
{
	// nothing to do?
	return 0;
}

static int at_response_qaudloop(struct pvt* pvt, const struct ast_str* const response)
{
	int aloop;
	if (at_parse_qaudloop(ast_str_buffer(response), &aloop)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	ast_verb(1, "[%s] Audio loop is %s\n", PVT_ID(pvt), S_COR(aloop, "enabled", "disabled"));
	return 0;
}

static int at_response_qaudmod(struct pvt* pvt, const struct ast_str* const response)
{
	static const char* const amodes[] = {
		"handset", "headset", "speaker", "off", "bluetooth", "none"
	};

	int amode;
	if (at_parse_qaudmod(ast_str_buffer(response), &amode)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	ast_verb(1, "[%s] Audio mode is %s\n", PVT_ID(pvt), enum2str_def((unsigned)amode, amodes, ITEMS_OF(amodes),"unknown"));
	return 0;	
}

static int at_response_cgmr_ex(struct pvt* pvt, const struct ast_str* const response)
{
	char* cgmr;
	if (at_parse_cgmr(ast_str_buffer(response), &cgmr)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	ast_verb(1, "[%s] Revision identification is %s\n", PVT_ID(pvt), cgmr);
	ast_string_field_set(pvt, firmware, cgmr);
	return 0;
}

static int at_response_cpcmreg(struct pvt* pvt, const struct ast_str* const response)
{
	int pcmreg;
	if (at_parse_cpcmreg(ast_str_buffer(response), &pcmreg)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	if (pcmreg)
		ast_log(LOG_NOTICE, "[%s] SimCom - Voice channel active", PVT_ID(pvt));
	else
		ast_log(LOG_NOTICE, "[%s] SimCom - Voice channel inactive", PVT_ID(pvt));
	return 0;
}

static int at_response_cnsmod(struct pvt* pvt, const struct ast_str* const response)
{
	int act;
	if (at_parse_cnsmod(ast_str_buffer(response), &act)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	if (act >=0) {
		ast_verb(1, "[%s] Access technology: %s\n", PVT_ID(pvt), sys_act2str(act));
		pvt_set_act(pvt, act);
	}
	return 0;
}

static int at_response_cring(struct pvt* pvt, const struct ast_str* const response)
{
	char* ring_type;
	if (at_parse_cring(ast_str_buffer(response), &ring_type)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	ast_verb(3, "[%s] Ring: %s\n", PVT_ID(pvt), ring_type);
	return 0;
}

static int at_response_psnwid(struct pvt* pvt, const struct ast_str* const response)
{
	int mcc, mnc;
	char *fnn, *snn;

	if (at_parse_psnwid(ast_str_buffer(response), &mcc, &mnc, &fnn, &snn)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}


	if (pvt->use_ucs2_encoding) {
		char fnn_utf8_str[50];
		const int fnn_nibbles = unhex(fnn, (uint8_t*)fnn);
		ssize_t res = ucs2_to_utf8((const uint16_t*)fnn, (fnn_nibbles + 1) / 4, fnn_utf8_str, STRLEN(fnn_utf8_str));
		if (res < 0) {
			ast_log(LOG_ERROR, "[%s] Error decoding full network name: %s\n", PVT_ID(pvt), fnn);
			return -1;
		}
		fnn_utf8_str[res] = '\000';
		ast_string_field_set(pvt, network_name, fnn_utf8_str);
	} else { // ASCII
		ast_string_field_set(pvt, network_name, fnn);
	}

	if (pvt->use_ucs2_encoding) {
		char snn_utf8_str[50];
		const int snn_nibbles = unhex(snn, (uint8_t*)snn);
		ssize_t res = ucs2_to_utf8((const uint16_t*)snn, (snn_nibbles + 1) / 4, snn_utf8_str, STRLEN(snn_utf8_str));
		if (res < 0) {
			ast_log(LOG_ERROR, "[%s] Error decoding short network name: %s\n", PVT_ID(pvt), snn);
			return -1;
		}
		snn_utf8_str[res] = '\000';
		ast_string_field_set(pvt, short_network_name, snn_utf8_str);
	} else { // ASCII
		ast_string_field_set(pvt, short_network_name, snn);
	}

	pvt->operator = mcc * 100 + mnc;
	ast_verb(1, "[%s] Operator: %s/%s\n", PVT_ID(pvt), pvt->network_name, pvt->short_network_name);
	ast_verb(1, "[%s] Registered PLMN: %d\n", PVT_ID(pvt), pvt->operator);

	return 0;
}

static int at_response_psuttz(struct pvt* pvt, const struct ast_str* const response)
{
	int year, month, day, hour, min, sec, dst, time_zone;

	if (at_parse_psuttz(ast_str_buffer(response), &year, &month, &day, &hour, &min, &sec, &time_zone, &dst)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	struct ast_str* module_time = ast_str_alloca(50);
	ast_str_set(&module_time, 100, 
		"%02d/%02d/%02d,%02d:%02d:%02d%+d",
		year % 100, month, day, hour, min, sec, time_zone);

	ast_verb(3, "[%s] Module time: %s\n", PVT_ID(pvt), ast_str_buffer(module_time));
	ast_string_field_set(pvt, module_time, ast_str_buffer(module_time));
	return 0;
}

static int at_response_revision(struct pvt* pvt, const struct ast_str* const response)
{
	char* rev;
	if (at_parse_revision(ast_str_buffer(response), &rev)) {
		ast_log(LOG_ERROR, "[%s] Error parsing '%s'\n", PVT_ID(pvt), ast_str_buffer(response));
		return -1;
	}

	ast_debug(1, "[%s] Firmware: %s\n", PVT_ID(pvt), rev);
	ast_string_field_set(pvt, firmware, rev);
	return 0;
}

static int check_at_res(at_res_t at_res)
{
	switch (at_res)
	{
		case RES_OK:
		case RES_ERROR:
		case RES_SMS_PROMPT:
		return 1;

		default:
		return 0;
	}
}

static void show_response(const struct pvt* const pvt, const at_queue_cmd_t* const ecmd, const struct ast_str* const result, at_res_t at_res)
{
	struct ast_str* const resc = escape_str(result);
	if (ecmd && ecmd->cmd == CMD_USER) {
		if (check_at_res(at_res)) {
			ast_verb(2, "[%s][%s] <-- [%s][%s]\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd), at_res2str(at_res), ast_str_buffer(resc));			
		}
		else {
			ast_verb(1, "[%s][%s] <-- [%s][%s]\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd), at_res2str(at_res), ast_str_buffer(resc));
		}
		goto show_done;
	}

	const int lvl = check_at_res(at_res)? 4 : 2;
	if (ecmd) {
		ast_debug(lvl, "[%s][%s] <-- [%s][%s]\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd), at_res2str(at_res), ast_str_buffer(resc));
	}
	else {
		ast_debug(lvl, "[%s] <-- [%s][%s]\n", PVT_ID(pvt), at_res2str(at_res), ast_str_buffer(resc));
	}

show_done:
	ast_free(resc);
}

/*!
 * \brief Do response
 * \param pvt -- pvt structure
 * \param iovcnt -- number of elements array pvt->d_read_iov
 * \param at_res -- result type
 * \retval  0 success
 * \retval -1 error
 */

int at_response(struct pvt* pvt, const struct ast_str* const result, at_res_t at_res)
{
	const size_t len = ast_str_strlen(result);

	const at_queue_task_t *task = at_queue_head_task(pvt);
	const at_queue_cmd_t *ecmd = at_queue_task_cmd(task);

	if (len) {
		show_response(pvt, ecmd, result, at_res);
		char* const str = ast_str_buffer(result);

		switch (at_res)
		{
			case RES_BOOT:
			case RES_CSSI:
			case RES_CSSU:
			case RES_SRVST:
			case RES_CVOICE:
			case RES_CPMS:
			case RES_CONF:
			case RES_DST:
			return 0;

			case RES_CMGS:
				{
					const int res = at_parse_cmgs(str);

					char payload[SMSDB_PAYLOAD_MAX_LEN];
					char dst[SMSDB_DST_MAX_LEN];
					ssize_t payload_len = smsdb_outgoing_part_put(task->uid, res, dst, payload);
					if (payload_len >= 0) {
						ast_verb (3, "[%s] Error payload: %.*s\n", PVT_ID(pvt), (int) payload_len, payload);
						start_local_report_channel(pvt, dst, payload, NULL, NULL, 1, 'i', NULL);
					}
				}
				return 0;

			case RES_OK:
				at_response_ok (pvt, at_res);
				return 0;

			case RES_QIND:
				/* An error here is not fatal. Just keep going. */
				at_response_qind(pvt, str);
				return 0;

			case RES_DSCI:
#ifdef HANDLE_DSCI			
				return at_response_dsci(pvt, str);
#else
				return 0;
#endif				

			case RES_CEND:
#ifdef HANDLE_CEND			
				return at_response_cend (pvt, str);
#else				
				return 0;
#endif				

			case RES_RCEND:
#ifdef HANDLE_RCEND			
				return at_response_rcend(pvt);
#else
				return 0;
#endif				

			case RES_CREG:
				/* An error here is not fatal. Just keep going. */
				at_response_creg(pvt, str, len);
				return 0;

			case RES_CEREG:
				return 0;

			case RES_COPS:
				/* An error here is not fatal. Just keep going. */
				at_response_cops(pvt, str);
				return 0;

			case RES_QSPN:
				at_response_qspn(pvt, str);
				return 0;

			case RES_QNWINFO:
				at_response_qnwinfo(pvt, str);
				return 0;

			case RES_CSQ:
				/* An error here is not fatal. Just keep going. */
				at_response_csq(pvt, result);
				break;

			case RES_CSQN:
				/* An error here is not fatal. Just keep going. */
				at_response_csqn(pvt, result);
				break;

			case RES_CMS_ERROR:
			case RES_ERROR:
				return at_response_error (pvt, at_res);

			case RES_RING:
				ast_log(LOG_NOTICE, "[%s] Receive RING\n", PVT_ID(pvt) );
				break;

			case RES_SMMEMFULL:
				return at_response_smmemfull (pvt);
/*
			case RES_CLIP:
				return at_response_clip (pvt, str, len);
*/
			case RES_CDSI:
				return at_response_cdsi (pvt, str);

			case RES_CMTI:
				return at_response_cmti (pvt, str);

			case RES_CMGR:
			case RES_CMGL:
			case RES_CMT:
			case RES_CBM:
			case RES_CDS:
				return at_response_msg(pvt, result, at_res);

			case RES_SMS_PROMPT:
				return at_response_sms_prompt (pvt);

			case RES_CUSD:
				/* An error here is not fatal. Just keep going. */
				at_response_cusd(pvt, result, 0);
				break;

			case RES_CLCC:
				return at_response_clcc(pvt, str);

			case RES_CCWA:
				return at_response_ccwa (pvt, str);

			case RES_BUSY:
				ast_log (LOG_ERROR, "[%s] Receive BUSY\n", PVT_ID(pvt));
				return 0;

			case RES_NO_DIALTONE:
				ast_log (LOG_ERROR, "[%s] Receive NO DIALTONE\n", PVT_ID(pvt));
				at_response_busy(pvt, AST_CONTROL_CONGESTION);
				break;

			case RES_NO_ANSWER:
				ast_debug(2, "[%s] Receive NO ANSWER\n", PVT_ID(pvt));
				return 0;

			case RES_NO_CARRIER:
				ast_debug(2, "[%s] Receive NO CARRIER\n", PVT_ID(pvt));
				return 0;

			case RES_CPIN:
				/* fatal */
				return at_response_cpin (pvt, str, len);

			case RES_CNUM:
				/* An error here is not fatal. Just keep going. */
				at_response_cnum (pvt, str);
				return 0;

			case RES_CSCA:
				/* An error here is not fatal. Just keep going. */
				at_response_csca (pvt, str);
				return 0;

			case RES_QTONEDET:
				at_response_qtonedet(pvt, result);
				return 0;

			case RES_DTMF:
			case RES_RXDTMF:
				at_response_dtmf(pvt, result);
				return 0;

			case RES_QPCMV:
				at_response_qpcmv(pvt, str, len);
				return 0;

			case RES_QLTS:
				at_response_qlts(pvt, result);
				return 0;

			case RES_CCLK:
				at_response_cclk(pvt, str, len);
				return 0;

			case RES_QRXGAIN:
				at_response_qrxgain(pvt, result);
				return 0;

			case RES_QMIC:
				at_response_qmic(pvt, result);
				return 0;

			case RES_CMICGAIN:
				at_response_cmicgain(pvt, result);
				return 0;

			case RES_COUTGAIN:
				at_response_coutgain(pvt, result);
				return 0;

			case RES_CTXVOL:
				at_response_ctxvol(pvt, result);
				return 0;

			case RES_CRXVOL:
				at_response_crxvol(pvt, result);
				return 0;

			case RES_CSMS:
				return at_response_csms(pvt, result);

			case RES_QAUDMOD:
				return at_response_qaudmod(pvt, result);

			case RES_QAUDLOOP:
				return at_response_qaudloop(pvt, result);

			case RES_CGMR:
				return at_response_cgmr_ex(pvt, result);

			case RES_CPCMREG:
				return at_response_cpcmreg(pvt, result);

			case RES_CNSMOD:
				return at_response_cnsmod(pvt, result);

			case RES_PARSE_ERROR:
				ast_log (LOG_ERROR, "[%s] Error parsing result\n", PVT_ID(pvt));
				return -1;

			case RES_CRING:
				return at_response_cring(pvt, result);

			case RES_PSNWID:
				return at_response_psnwid(pvt, result);

			case RES_PSUTTZ:
				return at_response_psuttz(pvt, result);

			case RES_REVISION:
				return at_response_revision(pvt, result);

			case RES_ICCID:
			case RES_QCCID:
				return at_response_xccid(pvt, result);

			case RES_UNKNOWN:
				if (ecmd) {
					switch (ecmd->cmd)
					{
						case CMD_AT_CGMI:
							ast_debug(2, "[%s] Got manufacturer info\n", PVT_ID(pvt));
							return at_response_cgmi(pvt, result);

						case CMD_AT_CGMM:
							ast_debug(2, "[%s] Got model info\n", PVT_ID(pvt));
							return at_response_cgmm(pvt, result);

						case CMD_AT_CGMR:
							ast_debug(2, "[%s] Got firmware info\n", PVT_ID(pvt));
							return at_response_cgmr(pvt, result);

						case CMD_AT_CGSN:
							ast_debug(2, "[%s] Got IMEI number\n", PVT_ID(pvt));
							return at_response_cgsn(pvt, result);

						case CMD_AT_CIMI:
							ast_debug(2, "[%s] Got IMSI number\n", PVT_ID(pvt));
							return at_response_cimi(pvt, result);

						case CMD_AT_CCID:
							ast_debug(2, "[%s] Got ICCID number\n", PVT_ID(pvt));
							return at_response_ccid(pvt, result);

						default:
							break;
					}
				}
				ast_debug (1, "[%s] Ignoring unknown result: '%.*s'\n", PVT_ID(pvt), (int) len, str);
				break;
			
			case COMPATIBILITY_RES_START_AT_MINUSONE:
				break;
		}
	}

	return 0;
}
