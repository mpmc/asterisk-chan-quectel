/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>
*/
#include "ast_config.h"

#include "memmem.h"

#include <stdio.h>			/* NULL */
#include <errno.h>			/* errno */
#include <stdlib.h>			/* strtol */

#include "at_parse.h"
#include "mutils.h"			/* ITEMS_OF() */
#include "chan_quectel.h"
#include "pdu.h"			/* pdu_parse() */
#include "error.h"

#/* */
static unsigned mark_line(char * line, const char * delimiters, char * pointers[])
{
	unsigned found = 0;

	for(; line[0] && delimiters[found]; line++)
	{
		if(line[0] == delimiters[found])
		{
			pointers[found] = line;
			found++;
		}
	}
	return found;
}

const char* at_qind2str(qind_t qind)
{
	static const char* qind_names[] = {
		"NONE",
		"CSQ",
		"ACT",
		"CCINFO"
	};
	return enum2str_def(qind, qind_names, ITEMS_OF(qind_names), "UNK");
}

/*!
 * \brief Parse a CNUM response
 * \param str -- string to parse (null terminated)
 * @note str will be modified when the CNUM message is parsed
 * \return NULL on error (parse error) or a pointer to the subscriber number
 */

char * at_parse_cnum (char* str)
{
	/*
	 * parse CNUM response in the following format:
	 * +CNUM: <name>,<number>,<type>
	 *   example
	 *   +CNUM: "Subscriber Number","+79139131234",145
	 *   +CNUM: "Subscriber Number","",145
	 *   +CNUM: "Subscriber Number",,145
	 */

	char delimiters[] = ":,,";
	char * marks[STRLEN(delimiters)];

	/* parse URC only here */
	if(mark_line(str, delimiters, marks) == ITEMS_OF(marks))
	{
		marks[1]++;
		if(marks[1][0] == '"')
			marks[1]++;
		if(marks[2][-1] == '"')
			marks[2]--;
		marks[2][0] = 0;
		return marks[1];
	}

	return NULL;
}

/*!
 * \brief Parse a COPS response
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * @note str will be modified when the COPS message is parsed
 * \return NULL on error (parse error) or a pointer to the provider name
 */

char* at_parse_cops (char* str)
{
	/*
	 * parse COPS response in the following format:
	 * +COPS: <mode>[,<format>,<oper>,<?>]
	 *
	 * example
	 *  +COPS: 0,0,"TELE2",0
	 */

	static const char delimiters[] = ":,,,";
	char* marks[STRLEN(delimiters)];

	/* parse URC only here */
	if (mark_line(str, delimiters, marks) == ITEMS_OF(marks)) {
		marks[2]++;
		if(marks[2][0] == '"')
			marks[2]++;
		if(marks[3][-1] == '"')
			marks[3]--;
		marks[3][0] = 0;
		/* Sometimes there is trailing garbage here;
		 * e.g. "Tele2@" or "Tele2<U+FFFD>" instead of "Tele2".
		 * Unsure why it happens (provider? quectel?), but it causes
		 * trouble later on too (at pbx_builtin_setvar_helper which
		 * is not encoding agnostic anymore, now that it uses json
		 * for messaging). See wdoekes/asterisk-chan-quectel
		 * GitHub issues #39 and #69. */
		while (marks[3] > marks[2] && (
				(unsigned char)marks[3][-1] < 32 ||
				(unsigned char)marks[3][-1] == '@' ||
				(unsigned char)marks[3][-1] >= 128)) {
			marks[3]--;
			marks[3][0] = 0;
		}
		return marks[2];
	}

	return NULL;
}

static char* strip_quoted(char* buf)
{
	return ast_strip_quoted(buf, "\"", "\"");
}

int at_parse_qspn(char* str, char** fnn, char** snn, char** spn)
{
	/*
		+QSPN: <FNN>,<SNN>,<SPN>,<alphabet>,<RPLMN>

		examples:

		+QSPN: "aaaaa","bbbbb","ccccc",0,"000000"
	*/

	static const char delimiters[] = ":,,,,";
	char* marks[STRLEN(delimiters)];

	/* parse URC only here */
	if (mark_line(str, delimiters, marks) == ITEMS_OF(marks)) {
		marks[0]++;
		if (marks[0][0] == ' ') marks[0]++;

		marks[1][0] = '\000';
		marks[1]++;

		marks[2][0] = '\000';
		marks[2]++;

		marks[3][0] = '\000';

		*spn = strip_quoted(marks[2]);
		*snn = strip_quoted(marks[1]);
		*fnn = strip_quoted(marks[0]);
		return 0;
	}

	return -1;
}

static int act2int(const char* act)
{
	static const struct {
		const char *act;
		int val;
	} ACTS[] = {
		{ "NONE", 0},
		{ "UNKNOWN", 0 },
		{ "GSM", 1 },
		{ "GPRS", 2},
		{ "EDGE", 3},
		{ "EGPRS", 3 },
		{ "WCDMA", 4 },
		{ "HSDPA", 5 },
		{ "HSUPA", 6 },
		{ "HSPA+", 7},
	 	{ "HSDPA&HSUPA", 7 },
		{ "TDD LTE", 8},
		{ "FDD LTE", 8},
		{ "LTE", 8 },
		{ "TDSCDMA", 9},
	 	{ "TD-SCDMA", 9 },
		{ "CDMA1X", 13},
		{ "CDMA", 13 },
		{ "EVDO", 14 },
		{ "CDMA1X AND HDR", 15},
		{ "HDR", 16 },
		{ "CDMA1X AND EHRPD", 24},
		{ "HDR-EHRPD", 24},
	};

	for(size_t idx = 0; idx < ITEMS_OF(ACTS); ++idx) {
		if (!strcmp(ACTS[idx].act, act)) {
			return ACTS[idx].val;
		}
	}

	return -1;
}

int at_parse_qnwinfo(char* str, int* act, int* oper, char** band, int* channel)
{
	/*
		+QNWINFO: <Act>,<oper>,<band>,<channel>
		+QNWINFO: No Service
	*/

	static const char delimiters[] = ":,,,";
	static const char NO_SERVICE[] = "No Service";

	char* marks[STRLEN(delimiters)];

	/* parse URC only here */
	const int nmarks = mark_line(str, delimiters, marks);
	if (nmarks == ITEMS_OF(marks)) {
		marks[0]++;
		if (marks[0][0] == ' ') marks[0]++;

		marks[1][0] = '\000';
		marks[1]++;

		marks[2][0] = '\000';
		marks[2]++;

		marks[3][0] = '\000';
		marks[3]++;

		const long ch = strtol(marks[3], (char**) NULL, 10);
		if (ch == 0) {
			return -1;
		}
		*channel = (int)ch;

		*band = strip_quoted(marks[2]);

		const long o = strtol(strip_quoted(marks[1]), (char**) NULL, 10);
		if (o == 0) {
			return -1;
		}
		*oper = (int)o;

		*act = act2int(strip_quoted(marks[0]));
		return 0;
	}
	else if (nmarks == 1) {
		const char* s = ast_strip(marks[0]+1);
		if (!strncmp(s, NO_SERVICE, STRLEN(NO_SERVICE))) {
			*act = -1;
			return 0;
		}
	}

	return -1;
}


/*!
 * \brief Parse a CREG response
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \param gsm_reg -- a pointer to a int
 * \param gsm_reg_status -- a pointer to a int
 * \param lac -- a pointer to a char pointer which will store the location area code in hex format
 * \param ci  -- a pointer to a char pointer which will store the cell id in hex format
 * @note str will be modified when the CREG message is parsed
 * \retval  0 success
 * \retval -1 parse error
 */
int at_parse_creg(char* str, unsigned, int* gsm_reg, int* gsm_reg_status, char** lac, char** ci)
{
	char* gsm_reg_str = NULL;

	*gsm_reg = 0;
	*gsm_reg_status = -1;
	*lac = NULL;
	*ci  = NULL;

	/*
	 * parse CREG response in the following formats:
	 *
	 *   +CREG: <n>,<stat>[,<LAC>,<ci>[,<Act>]] - response to AT+CREG?
	 *   +CREG: <stat>[,<LAC>,<ci>[,<Act>]]     - URC after AT-CREG=2
	 */

	static const char delimiters[] = ":,,,,";

	char* marks[STRLEN(delimiters)];
	const unsigned int n = mark_line(str, delimiters, marks);
	switch (n) {
		case 5:
		marks[1][0] = '\000';
		marks[2][0] = '\000';
		marks[3][0] = '\000';
		marks[4][0] = '\000';
		*ci = strip_quoted(marks[3]+1);
		*lac = strip_quoted(marks[2]+1);
		gsm_reg_str = strip_quoted(marks[1]+1);
		break;

		case 4:
		marks[1][0] = '\000';
		marks[2][0] = '\000';
		marks[3][0] = '\000';
		if (marks[1][1] == '"') {
			*ci = strip_quoted(marks[2]+1);
			*lac = strip_quoted(marks[1]+1);
			gsm_reg_str = strip_quoted(marks[0]+1);
		}
		else {
			*ci = strip_quoted(marks[3]+1);
			*lac = strip_quoted(marks[2]+1);
			gsm_reg_str = strip_quoted(marks[1]+1);
		}
		break;

		case 3:
		return -1;

		case 2:
		marks[1][0] = '\000';
		gsm_reg_str = strip_quoted(marks[1]+1);
		break;

		case 1:
		gsm_reg_str = strip_quoted(marks[0]+1);
		break;
	}

	if (gsm_reg_str) {
		errno = 0;
		*gsm_reg_status = (int)strtol(gsm_reg_str, (char**) NULL, 10);
		if (*gsm_reg_status == 0 && errno == EINVAL)
		{
			*gsm_reg_status = -1;
			return -1;
		}

		if (*gsm_reg_status == 1 || *gsm_reg_status == 5)
		{
			*gsm_reg = 1;
		}
	}

	return 0;
}

/*!
 * \brief Parse a CMTI notification
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * @note str will be modified when the CMTI message is parsed
 * \return -1 on error (parse error) or the index of the new sms message
 */

int at_parse_cmti(const char* str)
{
	int idx;

	/*
	 * Parse cmti info in the following format:
	 *
	 *   +CMTI: <mem>,<index>
	 * 
	 * Example:
	 * 
	 *   +CMTI: ,2
	 */

	return sscanf(str, "+CMTI:%*[^,],%u", &idx) == 1 ? idx : -1;
}

/*!
 * \brief Parse a CMSI notification
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * @note str will be modified when the CMTI message is parsed
 * \return -1 on error (parse error) or the index of the new sms message
 */

int at_parse_cdsi(const char* str)
{
	int index;

	/*
	 * parse cmti info in the following format:
	 * +CMTI: <mem>,<index>
	 */

	return sscanf(str, "+CDSI:%*[^,],%u", &index) == 1 ? index : -1;
}

static int parse_pdu(const char *str, size_t len, int *tpdu_type, char *sca, size_t sca_len, char *oa, size_t oa_len, char *scts, int *mr, int *st, char *dt, char *msg, size_t *msg_len, pdu_udh_t *udh)
{
	uint16_t msg16_tmp[256];

	int pdu_length = (unhex(str, (uint8_t*)str) + 1) / 2;
	if (pdu_length < 0) {
		chan_quectel_err = E_MALFORMED_HEXSTR;
		return -1;
	}

	int i = 0;
	int res = pdu_parse_sca((uint8_t*)(str + i), pdu_length - i, sca, sca_len);
	if (res < 0) {
		/* tpdu_parse_sca sets chan_quectel_err */
		return -1;
	}
	i += res;
	if (len > (size_t)(pdu_length - i)) {
		chan_quectel_err = E_INVALID_TPDU_LENGTH;
		return -1;
	}
	res = tpdu_parse_type((uint8_t*)(str + i), pdu_length - i, tpdu_type);
	if (res < 0) {
		/* tpdu_parse_type sets chan_quectel_err */
		return -1;
	}
	i += res;
	switch (PDUTYPE_MTI(*tpdu_type)) {
		case PDUTYPE_MTI_SMS_STATUS_REPORT:
		res = tpdu_parse_status_report((uint8_t*)(str + i), pdu_length - i, mr, oa, oa_len, scts, dt, st);
		if (res < 0) {
			/* tpdu_parse_status_report sets chan_quectel_err */
			return -1;
		}
		break;

		case PDUTYPE_MTI_SMS_DELIVER:
		res = tpdu_parse_deliver((uint8_t*)(str + i), pdu_length - i, *tpdu_type, oa, oa_len, scts, msg16_tmp, udh);
		if (res < 0) {
			/* tpdu_parse_deliver sets chan_quectel_err */
			return -1;
		}
		res = ucs2_to_utf8(msg16_tmp, res, msg, *msg_len);
		if (res < 0) {
			chan_quectel_err = E_PARSE_UCS2;
			return -1;
		}
		*msg_len = res;
		msg[res] = '\0';
		break;

		default:
		chan_quectel_err = E_INVALID_TPDU_TYPE;
		return -1;
	}
	return 0;
}

/*!
 * \brief Parse a CMGR message
 * \param str -- pointer to pointer of string to parse (null terminated)
 * \param len -- string lenght
 * \param number -- a pointer to a char pointer which will store the from number
 * \param text -- a pointer to a char pointer which will store the message text
 * @note str will be modified when the CMGR message is parsed
 * \retval  0 success
 * \retval -1 parse error
 */

int at_parse_cmgr(char *str, size_t len, int *tpdu_type, char *sca, size_t sca_len, char *oa, size_t oa_len, char *scts, int *mr, int *st, char *dt, char *msg, size_t *msg_len, pdu_udh_t *udh)
{
	/* skip "+CMGR:" */
	str += 6;
	len -= 6;

	/* skip leading spaces */
	while (len > 0 && *str == ' ') {
		++str;
		--len;
	}

	if (len <= 0) {
		chan_quectel_err = E_PARSE_CMGR_LINE;
		return -1;
	}
	if (str[0] == '"') {
		chan_quectel_err = E_DEPRECATED_CMGR_TEXT;
		return -1;
	}


	/*
	* parse cmgr info in the following PDU format
	* +CMGR: message_status,[address_text],TPDU_length<CR><LF>
	* SMSC_number_and_TPDU<CR><LF><CR><LF>
	* OK<CR><LF>
	*
	*	sample
	* +CMGR: 1,,31
	* 07911234567890F3040B911234556780F20008012150220040210C041F04400438043204350442<CR><LF><CR><LF>
	* OK<CR><LF>
	*/

	static const char delimiters[] = ",,\n";

	char *marks[STRLEN(delimiters)];
	char *end;

	if (mark_line(str, delimiters, marks) != ITEMS_OF(marks)) {
		chan_quectel_err = E_PARSE_CMGR_LINE;
	}

	const size_t tpdu_length = strtol(marks[1] + 1, &end, 10);
	if (tpdu_length <= 0 || end[0] != '\r') {
		chan_quectel_err = E_INVALID_TPDU_LENGTH;
		return -1;
	}

	return parse_pdu(marks[2] + 1, tpdu_length, tpdu_type, sca, sca_len, oa, oa_len, scts, mr, st, dt, msg, msg_len, udh);
}

int at_parse_cmgl(char *str, size_t len, int* idx, int *tpdu_type, char *sca, size_t sca_len, char *oa, size_t oa_len, char *scts, int *mr, int *st, char *dt, char *msg, size_t *msg_len, pdu_udh_t *udh)
{
	/* skip "+CMGL:" */
	str += 6;
	len -= 6;

	/* skip leading spaces */
	while (len > 0 && *str == ' ') {
		++str;
		--len;
	}

	if (len <= 0) {
		chan_quectel_err = E_PARSE_CMGR_LINE;
		return -1;
	}
	if (str[0] == '"') {
		chan_quectel_err = E_DEPRECATED_CMGR_TEXT;
		return -1;
	}


	/*
	* parse cmgr info in the following PDU format
	* +CMGL: idx,message_status,[address_text],TPDU_length<CR><LF>
	* SMSC_number_and_TPDU<CR><LF><CR><LF>
	* OK<CR><LF>
	*
	*	Exanoke
	* +CMGL: 0,1,,31
	* 07911234567890F3040B911234556780F20008012150220040210C041F04400438043204350442<CR><LF><CR><LF>
	* OK<CR><LF>
	*/

	static const char delimiters[] = ",,,\n";

	char *marks[STRLEN(delimiters)];
	char *end;

	if (mark_line(str, delimiters, marks) != ITEMS_OF(marks)) {
		chan_quectel_err = E_PARSE_CMGR_LINE;
	}

	*idx = strtol(str, &end, 10);
	if (*idx < 0 || end[0] != ',') {
		chan_quectel_err = E_UNKNOWN;
		return -1;		
	}
	
	const size_t tpdu_length = strtol(marks[2] + 1, &end, 10);
	if (tpdu_length <= 0 || end[0] != '\r') {
		chan_quectel_err = E_INVALID_TPDU_LENGTH;
		return -1;
	}

	return parse_pdu(marks[3] + 1, tpdu_length, tpdu_type, sca, sca_len, oa, oa_len, scts, mr, st, dt, msg, msg_len, udh);
}

/*!
 * \brief Parse a +CMGS notification
 * \param str -- string to parse (null terminated)
 * \return -1 on error (parse error) or the first integer value found
 * \todo FIXME: parse <mr>[,<scts>] value correctly
 */

int at_parse_cmgs (const char* str)
{
	int cmgs = -1;

	/*
	 * parse CMGS info in the following format:
	 * +CMGS:<mr>[,<scts>]
	 * (sscanf is lax about extra spaces)
	 * TODO: not ignore parse errors ;)
	 */
	sscanf (str, "+CMGS:%d", &cmgs);
	return cmgs;
}

 /*!
 * \brief Parse a CUSD answer
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * @note str will be modified when the CUSD string is parsed
 * \retval  0 success
 * \retval -1 parse error
 */

int at_parse_cusd (char* str, int * type, char** cusd, int * dcs)
{
	/*
	 * parse cusd message in the following format:
	 * +CUSD: <m>,[<str>,<dcs>]
	 *
	 * examples
	 *   +CUSD: 5
	 *   +CUSD: 0,"100,00 EURO, valid till 01.01.2010, you are using tariff "Mega Tariff". More informations *111#.",15
	 */

	char delimiters[] = ":,,";
	char * marks[STRLEN(delimiters)];
	unsigned count;

	*type = -1;
	*cusd = "";
	*dcs = -1;

	count = mark_line(str, delimiters, marks);
// 0, 1, 2, 3
	if(count > 0)
	{
		if(sscanf(marks[0] + 1, "%u", type) == 1)
		{
			if(count > 1)
			{
				marks[1]++;
				if(marks[1][0] == '"')
					marks[1]++;
				*cusd = marks[1];

				if(count > 2) {
					sscanf(marks[2] + 1, "%u", dcs);
					if(marks[2][-1] == '"')
						marks[2]--;
					marks[2][0] = 0;
				} else {
					int len = strlen(*cusd);
					if(len > 0 && (*cusd)[len - 1] == '"')
						(*cusd)[len-1] = 0;
				}
			}
			return 0;
		}
	}
	return -1;
}

/*!
 * \brief Parse a CPIN notification
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \return  2 if PUK required
 * \return  1 if PIN required
 * \return  0 if no PIN required
 * \return -1 on error (parse error) or card lock
 */

int at_parse_cpin (char* str, size_t len)
{
	static const struct {
		const char	* value;
		unsigned	length;
	} resp[] = {
		{ "READY", 5 },
		{ "SIM PIN", 7 },
		{ "SIM PUK", 7 },
	};

	unsigned idx;
	for(idx = 0; idx < ITEMS_OF(resp); idx++)
	{
		if(memmem (str, len, resp[idx].value, resp[idx].length) != NULL)
			return idx;
	}
	return -1;
}

/*!
 * \brief Parse +CSQ response
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

int at_parse_csq (const char* str, int* rssi)
{
	/*
	 * parse +CSQ response in the following format:
	 * +CSQ: <RSSI>,<BER>
	 */

	return sscanf (str, "+CSQ:%2d,", rssi) == 1 ? 0 : -1;
}

/*!
 * \brief Parse a ^RSSI notification
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \return -1 on error (parse error) or the rssi value
 */

int at_parse_rssi (const char* str)
{
	int rssi = -1;

	/*
	 * parse RSSI info in the following format:
	 * ^RSSI:<rssi>
	 */

	sscanf (str, "^RSSI:%d", &rssi);
	return rssi;
}

int at_parse_qind(char* str, qind_t* qind, char** params)
{
	static const char delimiters[] = "\"\",";
	char* marks[STRLEN(delimiters)];

	if (mark_line(str, delimiters, marks) == ITEMS_OF(marks)) {
		const char* qind_str = marks[0] + 1;
		marks[1][0] = '\000';

		if (!strcmp(qind_str, "csq")) {
			*qind = QIND_CSQ;
		}
		else if (!strcmp(qind_str, "act")) {
			*qind = QIND_ACT;
		}
		else if (!strcmp(qind_str, "ccinfo")) {
			*qind = QIND_CCINFO;
		}
		else {
			*qind = QIND_NONE;
		}

		*params = marks[2] + 1;
		return 0;
	}

	return -1;
}

int at_parse_qind_csq(const char* params, int* rssi)
{
	/*
	 * parse notification in the following format:
	 * +QIND: "csq",<RSSI>,<BER>
	 */

	return sscanf(params, "%d", rssi) == 1 ? 0 : -1;
}

int at_parse_qind_act(char* params, int* act)
{
	/*
	 * parse notification in the following format:
	 * +QIND: "act","<val>"
	 */

	*act = act2int(strip_quoted(params));
	return 0;
}

int at_parse_qind_cc(char* params, unsigned* call_idx, unsigned* dir, unsigned* state, unsigned* mode, unsigned* mpty, char** number, unsigned* toa)
{
	/*
	 * +QIND: "ccinfo",<idx>,<dir>,<state>,<mode>,<mpty>,<number>,<type>[,<alpha>]
	 * 
	 * examples
	 *  +QIND: "ccinfo",2,0,3,0,0,"XXXXXXXXX",129
	 *  +QIND: "ccinfo",2,0,-1,0,0,"XXXXXXXXX",129 [-1 => 7]
	 */
	static const char delimiters[] = ",,,,,,,";
	static const size_t nmarks = STRLEN(delimiters) - 1u;

	char* marks[STRLEN(delimiters)];

	if (mark_line(params, delimiters, marks) < nmarks) {
		return -1;
	}

	int cc_state;
	if (sscanf(params, "%u", call_idx) == 1 &&
		sscanf(marks[0] + 1, "%u", dir) == 1 &&
		sscanf(marks[1] + 1, "%d", &cc_state) == 1 &&
		sscanf(marks[2] + 1, "%u", mode) == 1 &&
		sscanf(marks[3] + 1, "%u", mpty) == 1 &&
		sscanf(marks[5] + 1, "%u", toa) == 1)
	{
		marks[4]++;
		if(marks[4][0] == '"')
			marks[4]++;
		if(marks[5][-1] == '"')
			marks[5]--;
		*number = marks[4];
		marks[5][0] = '\000';

		*state = (cc_state < 0)? CALL_STATE_RELEASED : (unsigned)cc_state;
		return 0;
	}

	return -1;
}

#/* */
int at_parse_csca(char* str, char ** csca)
{
	/*
	 * parse CSCA info in the following format:
	 * +CSCA: <SCA>,<TOSCA>
	 *  +CSCA: "+79139131234",145
	 *  +CSCA: "",145
	 */
	char delimiters[] = "\"\"";
	char * marks[STRLEN(delimiters)];

	if(mark_line(str, delimiters, marks) == ITEMS_OF(marks))
	{
		*csca = marks[0] + 1;
		marks[1][0] = 0;
		return 0;
	}

	return -1;
}

#/* */
int at_parse_dsci(char* str, unsigned* call_idx, unsigned* dir, unsigned* state, unsigned* call_type, char** number, unsigned* toa)
{
	/*
	 * ^DSCI: <id>,<dir>,<stat>,<type>,<number>,<num_type>[,<tone_info>]\r\n
	 
	 * examples
	 *
	 * ^DSCI: 2,1,4,0,+48XXXXXXXXX,145
	 * ^DSCI: 2,1,6,0,+48XXXXXXXXX,145
	 */

	static const char delimiters[] = ":,,,,,,";
	static const size_t nmarks = STRLEN(delimiters) - 1u;

	char* marks[STRLEN(delimiters)];

	if (mark_line(str, delimiters, marks) >= nmarks) {
		if (sscanf(marks[0] + 1, "%u", call_idx) == 1 &&
			sscanf(marks[1] + 1, "%u", dir) == 1 &&
			sscanf(marks[2] + 1, "%u", state) == 1 &&
			sscanf(marks[3] + 1, "%u", call_type) == 1 &&
			sscanf(marks[5] + 1, "%u", toa) == 1)
		{
			*number = marks[4] + 1;
			marks[5][0] = '\000';

			return 0;
		}
	}

	return -1;
}

#/* */
int at_parse_clcc(char* str, unsigned* call_idx, unsigned* dir, unsigned* state, unsigned* mode, unsigned* mpty, char** number, unsigned* toa)
{
	/*
	 * +CLCC:<id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]\r\n
	 *  ...
	 * +CLCC:<id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]\r\n
	 *  examples
	 *   +CLCC: 1,1,4,0,0,"",145
	 *   +CLCC: 1,1,4,0,0,"+79139131234",145
	 *   +CLCC: 1,1,4,0,0,"0079139131234",145
	 *   +CLCC: 1,1,4,0,0,"+7913913ABCA",145
	 */
	static const char delimiters[] = ":,,,,,,";
	char* marks[STRLEN(delimiters)];

	if (mark_line(str, delimiters, marks) == ITEMS_OF(marks)) {
		if (sscanf(marks[0] + 1, "%u", call_idx) == 1 &&
			sscanf(marks[1] + 1, "%u", dir) == 1 &&
			sscanf(marks[2] + 1, "%u", state) == 1 &&
			sscanf(marks[3] + 1, "%u", mode) == 1 &&
			sscanf(marks[4] + 1, "%u", mpty) == 1 &&
			sscanf(marks[6] + 1, "%u", toa) == 1)
		{
			marks[5]++;
			if(marks[5][0] == '"')
				marks[5]++;
			if(marks[6][-1] == '"')
				marks[6]--;
			*number = marks[5];
			marks[6][0] = '\000';

			return 0;
		}
	}

	return -1;
}

#/* */
int at_parse_ccwa(char* str, unsigned * class)
{
	/*
	 * CCWA may be in form:
	 *	in response of AT+CCWA=?
	 *		+CCWA: (0,1)
	 *	in response of AT+CCWA=?
	 *		+CCWA: <n>
	 *	in response of "AT+CCWA=[<n>[,<mode>[,<class>]]]"
	 *		+CCWA: <status>,<class1>
	 *
	 *	unsolicited result code
	 *		+CCWA: <number>,<type>,<class>,[<alpha>][,<CLI validity>[,<subaddr>,<satype>[,<priority>]]]
	 */
	char delimiters[] = ":,,";
	char * marks[STRLEN(delimiters)];

	/* parse URC only here */
	if(mark_line(str, delimiters, marks) == ITEMS_OF(marks))
	{
		if(sscanf(marks[2] + 1, "%u", class) == 1)
			return 0;
	}

	return -1;
}

int at_parse_qtonedet(char* str, int* dtmf)
{
	/*
		Example:

		+QTONEDET: 56
	*/

	return sscanf(str, "+QTONEDET:%d,", dtmf) == 1 ? 0 : -1;
}

int at_parse_qpcmv(char* str, int* enabled, int* mode)
{
	/*
		Example:

		+QPCMV: 1,2
	*/

	const int sr = sscanf(str, "+QPCMV:%d,%d", enabled, mode);
	switch (sr) {
		case 2:
			break;

		case 1:
			*mode = -1;
			break;

		default:
			return -1;
	}

	return 0;
}

int at_parse_qlts(char* str, char** ts)
{
	/*
		+QLTS: "2017/10/13,03:40:48+32,0"
	*/

	static const char delimiters[] = ":";
	char* marks[STRLEN(delimiters)];

	if (mark_line(str, delimiters, marks) == 1) {
		*ts = strip_quoted(marks[0]+1);
		return 0;
	}

	return -1;
}

int at_parse_cclk(char* str, char** ts)
{
	/*
		+CCLK: “08/11/26,10:15:02+32”
	*/

	static const char delimiters[] = ":";
	char* marks[STRLEN(delimiters)];

	if (mark_line(str, delimiters, marks) == 1) {
		*ts = strip_quoted(marks[0]+1);
		return 0;
	}

	return -1;
}

int at_parse_qrxgain(char* str, int* rxgain)
{
	/*
		Example:

		+QRXGAIN: 20577
	*/

	return sscanf(str, "+QRXGAIN:%d,", rxgain) == 1 ? 0 : -1;
}

