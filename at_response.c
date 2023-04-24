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


EXPORT_DEF const at_responses_t at_responses = { at_responses_list, 2, ITEMS_OF(at_responses_list), RES_MIN, RES_MAX};

/*!
 * \brief Get the string representation of the given AT response
 * \param res -- the response to process
 * \return a string describing the given response
 */

EXPORT_DEF const char* at_res2str (at_res_t res)
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

static int at_response_rcend(struct pvt * pvt)
{
	int call_index = 0;
	unsigned int duration   = 0;
	int end_status = 0;
	int cc_cause   = 0;
	struct cpvt * cpvt;

	cpvt = active_cpvt(pvt);
	if (cpvt) {

		if(CPVT_IS_SOUND_SOURCE(cpvt)) voice_disable(pvt);
		call_index = cpvt->call_idx;
		ast_debug(1, "[%s] CEND: call_index %d duration %d end_status %d cc_cause %d Line disconnected\n"
			, PVT_ID(pvt), call_index, duration, end_status, cc_cause);
		CPVT_RESET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
		PVT_STAT(pvt, calls_duration[cpvt->dir]) += duration;
		change_channel_state(cpvt, CALL_STATE_RELEASED, cc_cause);
	}

	return 0;
}

static int at_response_cend (struct pvt * pvt, const char* str)
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
		voice_disable(pvt);
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


static int at_response_ok (struct pvt* pvt, at_res_t res)
{
	const at_queue_task_t * task = at_queue_head_task (pvt);
	const at_queue_cmd_t * ecmd = at_queue_task_cmd(task);

	if(!ecmd)
	{
		ast_log (LOG_ERROR, "[%s] Received unexpected 'OK'\n", PVT_ID(pvt));
		return 0;
	}

	if(ecmd->res == RES_OK || ecmd->res == RES_CMGR)
	{
		switch (ecmd->cmd)
		{
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
				ast_debug (3, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));
				break;

			case CMD_AT_COPS_INIT:
				ast_debug (1, "[%s] Operator select parameters set\n", PVT_ID(pvt));
				break;

			case CMD_AT_CREG_INIT:
				ast_debug (1, "[%s] registration info enabled\n", PVT_ID(pvt));
				break;

			case CMD_AT_CREG:
				ast_debug (1, "[%s] registration query sent\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNUM:
				ast_debug (1, "[%s] Subscriber phone number query successed\n", PVT_ID(pvt));
				break;

			case CMD_AT_CVOICE:
				ast_debug (1, "[%s] Quectel has voice support\n", PVT_ID(pvt));

				pvt->is_simcom = 0;

				if (CONF_UNIQ(pvt, uac)) {
					at_enqueue_enable_uac(&pvt->sys_chan);
				} else {
					at_enqueue_enable_tty(&pvt->sys_chan);
				}
				break;

			case CMD_AT_CVOICE2:
				ast_debug (1, "[%s] Simcom has voice support\n", PVT_ID(pvt));

				pvt->is_simcom = 1;
				at_enqueue_qcrcind(&pvt->sys_chan);
				break;

			case CMD_AT_QPCMV_0:
				ast_debug(3, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));

				pvt->has_voice = 0;
				break;
			
			case CMD_AT_QPCMV_TTY:
			case CMD_AT_QPCMV_UAC:
			case CMD_AT_QCRCIND:
				ast_debug(3, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));

				pvt->has_voice = 1;
				break;

/*
			case CMD_AT_CLIP:
				ast_debug (1, "[%s] Calling line indication disabled\n", PVT_ID(pvt));
				break;
*/
			case CMD_AT_CSSN:
				ast_debug (1, "[%s] Supplementary Service Notification enabled successful\n", PVT_ID(pvt));
				break;

			case CMD_AT_CMGF:
				ast_debug (1, "[%s] SMS operation mode set to PDU\n", PVT_ID(pvt));
				break;

			case CMD_AT_CSCS:
				ast_debug (1, "[%s] UCS-2 text encoding enabled\n", PVT_ID(pvt));

				pvt->use_ucs2_encoding = 1;
				break;

			case CMD_AT_CPMS:
				ast_debug (1, "[%s] SMS storage location is established\n", PVT_ID(pvt));
				break;

			case CMD_AT_CNMI:
				ast_debug (1, "[%s] SMS new message indication enabled\n", PVT_ID(pvt));
				ast_debug (1, "[%s] Quectel has sms support\n", PVT_ID(pvt));

				pvt->has_sms = 1;

				if (!pvt->initialized)
				{
					pvt->timeout = DATA_READ_TIMEOUT;
					pvt->initialized = 1;
					ast_verb (3, "[%s] Quectel initialized and ready\n", PVT_ID(pvt));
				}
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
				ast_debug (1, "[%s] %s sent successfully for call id %d\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd), task->cpvt->call_idx);
				break;

			case CMD_AT_CFUN:
				/* in case of reset */
				pvt->ring = 0;
				pvt->dialing = 0;
				pvt->cwaiting = 0;
				break;

			case CMD_AT_CPCMREG1:
				ast_debug(1, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));
				if (!pvt->initialized) {
					pvt->timeout = DATA_READ_TIMEOUT;
					pvt->initialized = 1;
					ast_verb (3, "[%s] Quectel initialized and ready\n", PVT_ID(pvt));
				}
				break;

			case CMD_AT_CPCMREG0:
				ast_debug(1, "[%s] %s sent successfully\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));
				break;

			case CMD_AT_CHUP:
			case CMD_AT_QHUP:
     		case CMD_AT_CHLD_1x:
				CPVT_RESET_FLAGS(task->cpvt, CALL_FLAG_NEED_HANGUP);
				ast_debug (1, "[%s] Successful hangup for call idx %d\n", PVT_ID(pvt), task->cpvt->call_idx);
				break;

			case CMD_AT_CMGS:
				ast_debug (1, "[%s] Sending sms message in progress\n", PVT_ID(pvt));
				break;

			case CMD_AT_SMSTEXT:
				pvt->outgoing_sms = 0;
				pvt_try_restate(pvt);

				/* TODO: move to +CMGS: handler */
				ast_verb (3, "[%s] Successfully sent SMS message %p\n", PVT_ID(pvt), task);
				ast_log (LOG_NOTICE, "[%s] Successfully sent SMS message %p\n", PVT_ID(pvt), task);
				break;

			case CMD_AT_DTMF:
				ast_debug (1, "[%s] DTMF sent successfully for call idx %d\n", PVT_ID(pvt), task->cpvt->call_idx);
				break;

			case CMD_AT_CUSD:
				ast_verb (3, "[%s] Successfully sent USSD %p\n", PVT_ID(pvt), task);
				ast_log (LOG_NOTICE, "[%s] Successfully sent USSD %p\n", PVT_ID(pvt), task);
				break;

			case CMD_AT_COPS:
			case CMD_AT_QSPN:
				ast_debug (1, "[%s] Provider query successfully\n", PVT_ID(pvt));
				break;

			case CMD_AT_CMGR:
				ast_debug (1, "[%s] SMS message see later\n", PVT_ID(pvt));
				at_retrieve_next_sms(&pvt->sys_chan, at_cmd_suppress_error_mode(ecmd->flags));
				break;

			case CMD_AT_CMGD:
				ast_debug (1, "[%s] SMS message deleted successfully\n", PVT_ID(pvt));
				break;

			case CMD_AT_CSQ:
				ast_debug (1, "[%s] Got signal strength result\n", PVT_ID(pvt));
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

			case CMD_USER:
				break;

			default:
				ast_log (LOG_ERROR, "[%s] Received 'OK' for unhandled command '%s'\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));
				break;
		}
		at_queue_handle_result (pvt, res);
	}
	else {
		ast_log (LOG_ERROR, "[%s] Received 'OK' when expecting '%s', ignoring\n", PVT_ID(pvt), at_res2str (ecmd->res));
	}

	return 0;
}

static void log_cmd_response_error(const struct pvt* pvt, const at_queue_cmd_t *ecmd, const char *fmt, ...)
{
	va_list ap;
	char tempbuff[512];

	if (at_cmd_suppress_error_mode(ecmd->flags) == SUPPRESS_ERROR_ENABLED) {
		if (DEBUG_ATLEAST(1)) {
			ast_log(AST_LOG_DEBUG, "[%s] Command response error suppressed:\n", PVT_ID(pvt));
			va_start(ap, fmt);
			vsnprintf(tempbuff, 512, fmt, ap);
			va_end(ap);
			ast_log(AST_LOG_DEBUG, "%s", tempbuff);
		}

		return;
	}

	va_start(ap, fmt);
	vsnprintf(tempbuff, 512, fmt, ap);	
	ast_log(LOG_ERROR, "%s", tempbuff);
	va_end(ap);
}

/*!
 * \brief Handle ERROR response
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_error (struct pvt* pvt, at_res_t res)
{
	const at_queue_task_t * task = at_queue_head_task(pvt);
	const at_queue_cmd_t * ecmd = at_queue_task_cmd(task);

	if (ecmd && (ecmd->res == RES_OK || ecmd->res == RES_CMGR || ecmd->res == RES_SMS_PROMPT))
	{
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

			case CMD_AT_CREG:
				ast_debug (1, "[%s] Error getting registration info\n", PVT_ID(pvt));
				break;

			case CMD_AT_QINDCFG_CSQ:
			case CMD_AT_QINDCFG_ACT:
			case CMD_AT_QINDCFG_RING:
			case CMD_AT_QINDCFG_CC:
			case CMD_AT_DSCI:
			case CMD_AT_QCRCIND:
				ast_debug(1, "[%s] Error enabling indications\n", PVT_ID(pvt));
				break;

			case CMD_AT_CVOICE:
				ast_debug (1, "[%s] No Quectel voice support\n", PVT_ID(pvt));
				pvt->has_voice = 0;
                break;

			case CMD_AT_CVOICE2:
				ast_debug (1, "[%s] No Simcom voice support\n", PVT_ID(pvt));
				pvt->is_simcom = 0;

				if (!pvt->initialized) {
					if (at_enqueue_initialization(task->cpvt, CMD_AT_CNMI)) {
						log_cmd_response_error(pvt, ecmd, "[%s] Error schedule initialization commands\n", PVT_ID(pvt));
						goto e_return;
					}
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
				ast_debug (1, "[%s] Command '%s' failed\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));
				ast_debug (1, "[%s] No SMS support\n", PVT_ID(pvt));

				pvt->has_sms = 0;

				if (!pvt->initialized) {
					if (pvt->has_voice) {
						if (at_enqueue_initialization(task->cpvt, CMD_AT_CNMI)) {
							log_cmd_response_error(pvt, ecmd, "[%s] Error querying signal strength\n", PVT_ID(pvt));
							goto e_return;
						}

						pvt->timeout = DATA_READ_TIMEOUT;
						pvt->initialized = 1;
						/* FIXME: say 'initialized and ready' but disconnect */
						// ast_verb (3, "[%s] Quectel initialized and ready\n", PVT_ID(pvt));
					}
					goto e_return;
				}
				break;

			case CMD_AT_CSCS:
				ast_debug (1, "[%s] No UCS-2 encoding support\n", PVT_ID(pvt));

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
				if (!CPVT_TEST_FLAG(task->cpvt, CALL_FLAG_HOLD_OTHER) ||
						task->cpvt->state != CALL_STATE_INIT) {
					break;
				}
				/* fall through */
			case CMD_AT_D:
				log_cmd_response_error(pvt, ecmd, "[%s] Dial failed\n", PVT_ID(pvt));
				queue_control_channel (task->cpvt, AST_CONTROL_CONGESTION);
				break;

			case CMD_AT_CPCMREG1:
				log_cmd_response_error(pvt, ecmd, "[%s] %s Enable audio failed\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));
				break;

			case CMD_AT_CPCMREG0:
				log_cmd_response_error(pvt, ecmd, "[%s] %s Disable audio failed\n", PVT_ID(pvt), at_cmd2str(ecmd->cmd));
				break;

			case CMD_AT_CHUP:
			case CMD_AT_QHUP:
			case CMD_AT_CHLD_1x:
				log_cmd_response_error(pvt, ecmd, "[%s] Error sending hangup for call idx %d\n", PVT_ID(pvt), task->cpvt->call_idx);
				break;

			case CMD_AT_CMGR:
				// log_cmd_response_error(pvt, ecmd, "[%s] Error reading SMS message, resetting index\n", PVT_ID(pvt));
                pvt->incoming_sms_index = -1U;
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
						ast_verb (3, "[%s] Error payload: %.*s\n", PVT_ID(pvt), (int) payload_len, payload);
						channel_var_t vars[] =
						{
							{ "SMS_REPORT_PAYLOAD", payload },
							{ "SMS_REPORT_TS", "" },
							{ "SMS_REPORT_DT", "" },
							{ "SMS_REPORT_SUCCESS", "0" },
							{ "SMS_REPORT_TYPE", "i" },
							{ "SMS_REPORT", "" },
							{ NULL, NULL },
						};
						start_local_channel(pvt, "report", dst, vars);
					}
				}

				ast_verb (3, "[%s] Error sending SMS message %p\n", PVT_ID(pvt), task);
				log_cmd_response_error(pvt, ecmd, "[%s] Error sending SMS message %p %s\n", PVT_ID(pvt), task, at_cmd2str (ecmd->cmd));
				break;

			case CMD_AT_DTMF:
				log_cmd_response_error(pvt, ecmd, "[%s] Error sending DTMF\n", PVT_ID(pvt));
				break;

			case CMD_AT_COPS:
			case CMD_AT_QSPN:
				ast_debug (1, "[%s] Could not get provider name\n", PVT_ID(pvt));
				break;

			case CMD_AT_CLVL:
				ast_debug (1, "[%s] Audio level synchronization failed at step %d/%d\n", PVT_ID(pvt), pvt->volume_sync_step, VOLUME_SYNC_DONE-1);
				pvt->volume_sync_step = VOLUME_SYNC_BEGIN;
				break;

			case CMD_AT_CUSD:
				ast_verb (3, "[%s] Error sending USSD %p\n", PVT_ID(pvt), task);
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

			default:
				log_cmd_response_error(pvt, ecmd, "[%s] Received 'ERROR' for unhandled command '%s'\n", PVT_ID(pvt), at_cmd2str (ecmd->cmd));
				break;
		}
		at_queue_handle_result (pvt, res);
	}
	else if (ecmd)
	{
		switch (ecmd->cmd) {
		case CMD_AT_CMGR:
			at_retrieve_next_sms(&pvt->sys_chan, at_cmd_suppress_error_mode(ecmd->flags));
			break;
		default:
			log_cmd_response_error(pvt, ecmd, "[%s] Received 'ERROR' when expecting '%s', ignoring\n", PVT_ID(pvt), at_res2str (ecmd->res));
			break;
		}
	}
	else
	{
		log_cmd_response_error(pvt, ecmd, "[%s] Received unexpected 'ERROR'\n", PVT_ID(pvt));
	}

	return 0;

e_return:
	at_queue_handle_result (pvt, res);

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
			NULL, NULL);
#else /* 12- */
	struct ast_channel * channel = new_channel(
			pvt, AST_STATE_RING, number, call_idx, CALL_DIR_INCOMING, state,
			pvt->has_subscriber_number ? pvt->subscriber_number : CONF_SHARED(pvt, exten),
			NULL);
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

		if (mpty == 0 || mpty == 1) {
			if(mpty)
				CPVT_SET_FLAGS(cpvt, CALL_FLAG_MULTIPARTY);
			else
				CPVT_RESET_FLAGS(cpvt, CALL_FLAG_MULTIPARTY);
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
			if (cpvt) {
				cpvt->call_idx = (short)call_idx;
				change_channel_state(cpvt, state, 0);				
			} else {
				at_enqueue_hangup(&pvt->sys_chan, call_idx, AST_CAUSE_CALL_REJECTED);
				ast_log(LOG_ERROR, "[%s] answered unexisting incoming call - idx:%d, hanging up!\n", PVT_ID(pvt), call_idx);
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
		break; // do nothing

		case CALL_STATE_DIALING:
		{
			pvt->dialing = 1;
			pvt->cwaiting = 0;
			pvt->ring = 0;

			if (pvt->is_simcom) {
				sleep(1);
				voice_enable(pvt);
			}
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
	int val;
	int res;

	qind_t qind;
	char* params;

	ast_debug(2, "[%s] QIND - %s\n", PVT_ID(pvt), str);

	res = at_parse_qind(str, &qind, &params);
	if (res < 0) {
		return -1;
	}

	ast_verb(1, "[%s] QIND(%s) - %s\n", PVT_ID(pvt), at_qind2str(qind), params);

	switch(qind) {
		case QIND_CSQ:
		{
			int val;
			res = at_parse_qind_csq(params, &val);
			if (res < 0) {
				ast_debug(1, "[%s] Failed to parse CSQ - %s\n", PVT_ID(pvt), params);
				break;
			}
			pvt->rssi = val;
			return 0;
		}

		case QIND_ACT:
		{
			res = at_parse_qind_act(params, &val);
			if (res < 0) {
				ast_debug(1, "[%s] Failed to parse ACT - %s\n", PVT_ID(pvt), params);
				break;
			}
			pvt->linkmode = val;
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
	char    csca_utf8_str[20];
	ssize_t res;

	if(at_parse_csca(str, &csca)) {
		ast_debug(1, "[%s] Could not parse CSCA response '%s'\n", PVT_ID(pvt), str);
		return -1;
	}

	if (pvt->use_ucs2_encoding) {
		const int csca_nibbles = unhex(csca, (uint8_t*)csca);
		res = ucs2_to_utf8((const uint16_t*)csca, (csca_nibbles + 1) / 4, csca_utf8_str, sizeof(csca_utf8_str) - 1);
	} else { // ASCII
		ast_log(LOG_NOTICE, "[%s] CSCA ASCII\n", PVT_ID(pvt));
		res = strlen(csca);
		if ((size_t)res > sizeof(csca_utf8_str) - 1) {
			res = -1;
		} else {
			memcpy(csca_utf8_str, csca, res);
		}
	}

	if (res < 0) {
		return -1;
	}
	csca_utf8_str[res] = '\0';
	ast_copy_string(pvt->sms_scenter, csca_utf8_str, sizeof(pvt->sms_scenter));
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
	if(n == 1)
		return 0;
	else if (n == 2)
	{
		if ((class & CCWA_CLASS_VOICE) && (status == CCWA_STATUS_NOT_ACTIVE || status == CCWA_STATUS_ACTIVE))
		{
			pvt->has_call_waiting = status == CCWA_STATUS_ACTIVE ? 1 : 0;
			ast_log (LOG_NOTICE, "Call waiting is %s on device %s\n", status ? "enabled" : "disabled", PVT_ID(pvt));
		}
		return 0;
	}

	if (pvt->initialized)
	{
//		if (sscanf (str, "+CCWA: \"%*[+0-9*#ABCabc]\",%*d,%d", &class) == 1)
		if (at_parse_ccwa(str, &class) == 0)
		{
//			if (CONF_SHARED(pvt, callwaiting) != CALL_WAITING_DISALLOWED && class == CCWA_CLASS_VOICE)
			if (class == CCWA_CLASS_VOICE)
			{
				pvt->cwaiting = 1;
				request_clcc(pvt);
			}
		}
		else
			ast_log (LOG_ERROR, "[%s] can't parse CCWA line '%s'\n", PVT_ID(pvt), str);
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
at_poll_sms (struct pvt *pvt)
{
	/* poll all SMSs stored in device */
	if (CONF_SHARED(pvt, disablesms) == 0)
	{
		int i;

		for (i = 0; i != SMS_INDEX_MAX; i++)
		{
			if (at_enqueue_retrieve_sms(&pvt->sys_chan, i, SUPPRESS_ERROR_ENABLED))
			{
				ast_log (LOG_ERROR, "[%s] Error sending CMGR to retrieve SMS message #%d\n", PVT_ID(pvt), i);
				return -1;
			}
		}
		return 0;
	}
	else
	{
		return -1;
	}
}

/*!
 * \brief Handle +CMTI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cmti (struct pvt* pvt, const char* str)
{
// FIXME: check format in PDU mode
	int index = at_parse_cmti (str);

	if (CONF_SHARED(pvt, disablesms))
	{
		ast_log (LOG_WARNING, "[%s] SMS reception has been disabled in the configuration.\n", PVT_ID(pvt));
		return 0;
	}

	if (index > -1)
	{
		ast_debug (1, "[%s] Incoming SMS message\n", PVT_ID(pvt));

		if (at_enqueue_retrieve_sms(&pvt->sys_chan, index, SUPPRESS_ERROR_DISABLED))
		{
			ast_log (LOG_ERROR, "[%s] Error sending CMGR to retrieve SMS message\n", PVT_ID(pvt));
			return -1;
		}
	}
	else
	{
		/* Not sure why this happens, but we don't want to disconnect standing calls.
		 * [Jun 14 19:57:57] ERROR[3056]: at_response.c:1173 at_response_cmti:
		 *   [m1-1] Error parsing incoming sms message alert '+CMTI: "SM",-1' */
		ast_log(LOG_WARNING, "[%s] Error parsing incoming sms message alert '%s', ignoring\n", PVT_ID(pvt), str);
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
	int index = at_parse_cdsi (str);

	if (CONF_SHARED(pvt, disablesms))
	{
		ast_log (LOG_WARNING, "[%s] SMS reception has been disabled in the configuration.\n", PVT_ID(pvt));
		return 0;
	}

	if (index > -1)
	{
		ast_debug (1, "[%s] Incoming SMS message\n", PVT_ID(pvt));

		if (at_enqueue_retrieve_sms(&pvt->sys_chan, index, SUPPRESS_ERROR_DISABLED))
		{
			ast_log (LOG_ERROR, "[%s] Error sending CMGR to retrieve SMS message\n", PVT_ID(pvt));
			return -1;
		}
	}
	else
	{
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

static int at_response_cmgr(struct pvt* pvt, char* str, size_t len)
{
	char oa[512] = "", sca[512] = "";
	char scts[64], dt[64];
	int mr, st;
	char msg[4096];
	int res;
	char text_base64[40800];
	size_t msg_len = sizeof(msg);
	int tpdu_type;
	pdu_udh_t udh;
	pdu_udh_init(&udh);
	char fullmsg[160 * 255];
	int fullmsg_len;
	int csms_cnt;
	char buf[512];
	char payload[SMSDB_PAYLOAD_MAX_LEN];
	ssize_t payload_len;
	int status_report[256];

	const struct at_queue_cmd * ecmd = at_queue_head_cmd(pvt);

	if (ecmd) {
		if (ecmd->res == RES_CMGR || ecmd->cmd == CMD_USER) {
			at_queue_handle_result (pvt, RES_CMGR);

			res = at_parse_cmgr(str, len, &tpdu_type, sca, sizeof(sca), oa, sizeof(oa), scts, &mr, &st, dt, msg, &msg_len, &udh);
			if (res < 0) {
				ast_log(LOG_WARNING, "[%s] Error parsing incoming message: %s\n", PVT_ID(pvt), error2str(chan_quectel_err));
				goto receive_next_no_delete;
			}
			switch (PDUTYPE_MTI(tpdu_type)) {
			case PDUTYPE_MTI_SMS_STATUS_REPORT:
				ast_verb(1, "[%s] Got status report with ref %d from %s and status code %d\n", PVT_ID(pvt), mr, oa, st);
				snprintf(buf, 64, "Delivered\r\nForeignID: %d", mr);
				payload_len = smsdb_outgoing_part_status(pvt->imsi, oa, mr, st, status_report, payload);
				if (payload_len >= 0) {
					int success = 1;
					char status_report_str[255 * 4 + 1];
					int srroff = 0;
					for (int i = 0; status_report[i] != -1; ++i) {
						success &= !(status_report[i] & 0x40);
						sprintf(status_report_str + srroff, "%03d,", status_report[i]);
						srroff += 4;
					}
					status_report_str[srroff] = '\0';
					ast_verb(1, "[%s] Success: %d; Payload: %.*s; Report string: %s\n", PVT_ID(pvt), success, (int) payload_len, payload, status_report_str);
					payload[payload_len] = '\0';
					channel_var_t vars[] =
					{
						{ "SMS_REPORT_PAYLOAD", payload } ,
						{ "SMS_REPORT_TS", scts },
						{ "SMS_REPORT_DT", dt },
						{ "SMS_REPORT_SUCCESS", success ? "1" : "0" },
						{ "SMS_REPORT_TYPE", "e" },
						{ "SMS_REPORT", status_report_str },
						{ NULL, NULL },
					};
					start_local_channel(pvt, "report", oa, vars);
				}
				break;
			case PDUTYPE_MTI_SMS_DELIVER:
				ast_debug (1, "[%s] Successfully read SM\n", PVT_ID(pvt));
				if (udh.parts > 1) {
					ast_verb (1, "[%s] Got SM part from %s: '%s'; [ref=%d, parts=%d, order=%d]\n", PVT_ID(pvt), oa, msg, udh.ref, udh.parts, udh.order);
					csms_cnt = smsdb_put(pvt->imsi, oa, udh.ref, udh.parts, udh.order, msg, fullmsg);
					if (csms_cnt <= 0) {
						ast_log(LOG_ERROR, "[%s] Error putting SMS to SMSDB\n", PVT_ID(pvt));
						goto receive_as_is;
					}
					if (csms_cnt < udh.parts) {
						ast_verb (1, "[%s] Waiting for following parts\n", PVT_ID(pvt));
						goto receive_next;
					}
					fullmsg_len = strlen(fullmsg);
				} else {
receive_as_is:
					ast_verb (1, "[%s] Got single SM from %s: '%s'\n", PVT_ID(pvt), oa, msg);
					strncpy(fullmsg, msg, msg_len);
					fullmsg[msg_len] = '\0';
					fullmsg_len = msg_len;
				}

				ast_verb (1, "[%s] Got full SMS from %s: '%s'\n", PVT_ID(pvt), oa, fullmsg);
				ast_base64encode (text_base64, (unsigned char*)fullmsg, fullmsg_len, sizeof(text_base64));

				{
					channel_var_t vars[] =
					{
						{ "SMS", fullmsg } ,
						{ "SMS_BASE64", text_base64 },
						{ "SMS_TS", scts },
						{ NULL, NULL },
					};
					start_local_channel (pvt, "sms", oa, vars);
				}
				break;
			}
		}
		else
		{
			ast_log (LOG_ERROR, "[%s] Received '+CMGR' when expecting '%s' response to '%s', ignoring\n", PVT_ID(pvt),
					at_res2str (ecmd->res), at_cmd2str (ecmd->cmd));
		}
receive_next:
		if (CONF_SHARED(pvt, autodeletesms) && pvt->incoming_sms_index != -1U)
		{
			at_enqueue_delete_sms(&pvt->sys_chan, pvt->incoming_sms_index);
		}
receive_next_no_delete:
		at_retrieve_next_sms(&pvt->sys_chan, at_cmd_suppress_error_mode(ecmd->flags));
	}
	else {
		ast_log(LOG_WARNING, "[%s] Received unexpected '+CMGR'\n", PVT_ID(pvt));
	}

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

static int at_response_cusd(struct pvt* pvt, char* str, size_t len)
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

	if (at_parse_cusd(str, &type, &cusd, &dcs)) {
		ast_verb(1, "[%s] Error parsing CUSD: '%.*s'\n", PVT_ID(pvt), (int) len, str);
		return -1;
	}

	if(type < 0 || type >= (int)ITEMS_OF(types)) {
		ast_log(LOG_WARNING, "[%s] Unknown CUSD type: %d\n", PVT_ID(pvt), type);
	}

	typestr = enum2str(type, types, ITEMS_OF(types));

	typebuf[0] = type + '0';
	typebuf[1] = 0;

	// sanitize DCS
	if (dcs & 0x40) {
		dcs = (dcs & 0xc) >> 2;
		if (dcs == 3) dcs = 0;
	} else {
		dcs = 0;
	}

	ast_verb (1, "[%s] USSD DCS=%d (0: gsm7, 1: ascii, 2: ucs2)\n", PVT_ID(pvt), dcs);
	if (dcs == 0) { // GSM-7
		uint16_t out_ucs2[1024];
		int cusd_nibbles = unhex(cusd, (uint8_t*)cusd);
		res = gsm7_unpack_decode(cusd, cusd_nibbles, out_ucs2, sizeof(out_ucs2) / 2, 0, 0, 0);
		if (res < 0) {
			return -1;
		}
		res = ucs2_to_utf8(out_ucs2, res, cusd_utf8_str, sizeof(cusd_utf8_str) - 1);
	} else if (dcs == 1) { // ASCII
		res = strlen(cusd);
		if ((size_t)res > sizeof(cusd_utf8_str) - 1) {
			res = -1;
		} else {
			memcpy(cusd_utf8_str, cusd, res);
		}
	} else if (dcs == 2) { // UCS-2
		int cusd_nibbles = unhex(cusd, (uint8_t*)cusd);
		res = ucs2_to_utf8((const uint16_t*)cusd, (cusd_nibbles + 1) / 4, cusd_utf8_str, sizeof(cusd_utf8_str) - 1);
	} else {
		res = -1;
	}
	if (res < 0) {
		return -1;
	}
	cusd_utf8_str[res] = '\0';

	ast_verb (1, "[%s] Got USSD type %d '%s': '%s'\n", PVT_ID(pvt), type, typestr, cusd_utf8_str);
	ast_base64encode (text_base64, (unsigned char*)cusd_utf8_str, res, sizeof(text_base64));

	{
		channel_var_t vars[] =
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
			ast_log (LOG_ERROR, "Quectel %s needs PIN code!\n", PVT_ID(pvt));
			break;
		case 2:
			ast_log (LOG_ERROR, "Quectel %s needs PUK code!\n", PVT_ID(pvt));
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

/*!
 * \brief Handle +CSQ response Here we get the signal strength and bit error rate
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */
static int at_response_csq (struct pvt* pvt, const char* str)
{
	int rssi;
	int rv = at_parse_csq (str, &rssi);

	if(rv)
		ast_debug (2, "[%s] Error parsing +CSQ result '%s'\n", PVT_ID(pvt), str);
	else
		pvt->rssi = rssi;
	return rv;
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
		ast_copy_string (pvt->subscriber_number, number, sizeof (pvt->subscriber_number));
		if(pvt->subscriber_number[0] != 0)
			pvt->has_subscriber_number = 1;
		return 0;
	}

	ast_copy_string (pvt->subscriber_number, "Unknown", sizeof (pvt->subscriber_number));
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

	if (provider_name) {
		ast_copy_string(pvt->provider_name, provider_name, sizeof(pvt->provider_name));
		ast_verb(1, "[%s] COPS - '%s'\n", PVT_ID(pvt), pvt->provider_name);
		return 0;
	}

	ast_copy_string(pvt->provider_name, "NONE", sizeof(pvt->provider_name));
	ast_verb(1, "[%s] COPS - '%s'\n", PVT_ID(pvt), pvt->provider_name);

	return -1;
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

	ast_verb(1, "[%s] QSPN - FNN:'%s' SNN:'%s' SPN:'%s'\n", PVT_ID(pvt), fnn, snn, spn);
	ast_copy_string(pvt->provider_name, spn, sizeof(pvt->provider_name));
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

	if (at_parse_creg(str, len, &gsm_reg, &pvt->gsm_reg_status, &lac, &ci)) {
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
			if (at_enqueue_qspn(&pvt->sys_chan)) {
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
	}
	else {
		pvt->gsm_registered = 0;
	}

	if (lac) {
		ast_copy_string(pvt->location_area_code, lac, sizeof(pvt->location_area_code));
	}

	if (ci) {
		ast_copy_string(pvt->cell_id, ci, sizeof(pvt->cell_id));
	}

	if (lac || ci) {
		ast_verb(1, "[%s] CREG - LAC:%s CID:%s\n", PVT_ID(pvt), pvt->location_area_code, pvt->cell_id);
	}

	return 0;
}

/*!
 * \brief Handle AT+CGMI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgmi (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->manufacturer, str, sizeof (pvt->manufacturer));

	return 0;
}

/*!
 * \brief Handle AT+CGMM response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

#/* */
static int at_response_cgmm (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->model, str, sizeof (pvt->model));

	return 0;
}

/*!
 * \brief Handle AT+CGMR response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgmr (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->firmware, str, sizeof (pvt->firmware));

	return 0;
}

/*!
 * \brief Handle AT+CGSN response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgsn (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->imei, str, sizeof (pvt->imei));

	return 0;
}

/*!
 * \brief Handle AT+CIMI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cimi (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->imsi, str, sizeof (pvt->imsi));

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

/*!
 * \brief Do response
 * \param pvt -- pvt structure
 * \param iovcnt -- number of elements array pvt->d_read_iov
 * \param at_res -- result type
 * \retval  0 success
 * \retval -1 error
 */

int at_response(struct pvt* pvt, const struct iovec* iov, int iovcnt, at_res_t at_res)
{
	char* str;
	size_t len;
	const at_queue_task_t *task = at_queue_head_task(pvt);
	const at_queue_cmd_t *ecmd = at_queue_task_cmd(task);

	if(iov[0].iov_len + iov[1].iov_len > 0) {
		len = iov[0].iov_len + iov[1].iov_len - 1;

		if (iovcnt == 2) {
			ast_debug(5, "[%s] iovcnt == 2\n", PVT_ID(pvt));

			str = alloca(len + 1);
			memcpy(str, iov[0].iov_base, iov[0].iov_len);
			memcpy(str + iov[0].iov_len, iov[1].iov_base, iov[1].iov_len);
		}
		else {
			str = iov[0].iov_base;
		}
		str[len] = '\0';

// 		ast_debug (5, "[%s] [%.*s]\n", PVT_ID(pvt), (int) len, str);

		if(ecmd && ecmd->cmd == CMD_USER) {
			ast_verb(1, "[%s] Got Response for user's command:'%s'\n", PVT_ID(pvt), str);
			ast_log(LOG_NOTICE, "[%s] Got Response for user's command:'%s'\n", PVT_ID(pvt), str);
		}

		switch (at_res)
		{
			case RES_BOOT:
			case RES_CSSI:
			case RES_CSSU:
			case RES_SRVST:
			case RES_CVOICE:
			case RES_CPMS:
			case RES_CONF: 
			return 0;

			case RES_CMGS:
				{
					int res = at_parse_cmgs(str);

					char payload[SMSDB_PAYLOAD_MAX_LEN];
					char dst[SMSDB_DST_MAX_LEN];
					ssize_t payload_len = smsdb_outgoing_part_put(task->uid, res, dst, payload);
					if (payload_len >= 0) {
						ast_verb (3, "[%s] Error payload: %.*s\n", PVT_ID(pvt), (int) payload_len, payload);
						channel_var_t vars[] =
						{
							{ "SMS_REPORT_PAYLOAD", payload },
							{ "SMS_REPORT_TS", "" },
							{ "SMS_REPORT_DT", "" },
							{ "SMS_REPORT_SUCCESS", "1" },
							{ "SMS_REPORT_TYPE", "i" },
							{ "SMS_REPORT", "" },
							{ NULL, NULL },
						};
						start_local_channel(pvt, "report", dst, vars);
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
				return at_response_dsci(pvt, str);

			case RES_CEND:
				return at_response_cend (pvt, str);

			case RES_RCEND:
				return at_response_rcend(pvt);

			case RES_CREG:
				/* An error here is not fatal. Just keep going. */
				at_response_creg (pvt, str, len);
				return 0;

			case RES_COPS:
				/* An error here is not fatal. Just keep going. */
				at_response_cops(pvt, str);
				return 0;

			case RES_QSPN:
				at_response_qspn(pvt, str);
				return 0;

			case RES_CSQ:
				/* An error here is not fatal. Just keep going. */
				at_response_csq (pvt, str);
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
				return at_response_cmgr(pvt, str, len);

			case RES_SMS_PROMPT:
				return at_response_sms_prompt (pvt);

			case RES_CUSD:
				/* An error here is not fatal. Just keep going. */
				at_response_cusd (pvt, str, len);
				break;

			case RES_CLCC:
				return at_response_clcc (pvt, str);

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
				ast_log (LOG_NOTICE, "[%s] Receive NO ANSWER\n", PVT_ID(pvt));
				return 0;

			case RES_NO_CARRIER:
				ast_log (LOG_NOTICE, "[%s] Receive NO CARRIER\n", PVT_ID(pvt));
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

			case RES_PARSE_ERROR:
				ast_log (LOG_ERROR, "[%s] Error parsing result\n", PVT_ID(pvt));
				return -1;

			case RES_UNKNOWN:
				if (ecmd) {
					switch (ecmd->cmd)
					{
						case CMD_AT_CGMI:
							ast_debug (1, "[%s] Got AT_CGMI data (manufacturer info)\n", PVT_ID(pvt));
							return at_response_cgmi (pvt, str);

						case CMD_AT_CGMM:
							ast_debug (1, "[%s] Got AT_CGMM data (model info)\n", PVT_ID(pvt));
							return at_response_cgmm (pvt, str);

						case CMD_AT_CGMR:
							ast_debug (1, "[%s] Got AT+CGMR data (firmware info)\n", PVT_ID(pvt));
							return at_response_cgmr (pvt, str);

						case CMD_AT_CGSN:
							ast_debug (1, "[%s] Got AT+CGSN data (IMEI number)\n", PVT_ID(pvt));
							return at_response_cgsn (pvt, str);

						case CMD_AT_CIMI:
							ast_debug (1, "[%s] Got AT+CIMI data (IMSI number)\n", PVT_ID(pvt));
							return at_response_cimi (pvt, str);
						default:
							break;
					}
				}
				ast_debug (1, "[%s] Ignoring unknown result: '%.*s'\n", PVT_ID(pvt), (int) len, str);
				break;
		}
	}

	return 0;
}
