/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_QUECTEL_AT_RESPONSE_H_INCLUDED
#define CHAN_QUECTEL_AT_RESPONSE_H_INCLUDED

struct pvt;
struct iovec;

/* AT_RESPONSES_TABLE */
#define AT_RES_AS_ENUM(res, desc, str) RES_ ## res,
#define AT_RES_AS_STRUCTLIST(res, desc, str) {RES_ ## res, desc, str, (sizeof(str)-1)},

#define AT_RESPONSES_TABLE(_) \
	_( PARSE_ERROR,	"PARSE ERROR",	"") \
	_( UNKNOWN,		"UNK",			"") \
\
	_( BOOT,        "^BOOT",        "^BOOT:") \
	_( BUSY,        "BUSY",         "BUSY\r") \
	_( COPS,        "+COPS",        "+COPS:") \
\
	_( CMGR,        "+CMGR",        "+CMGR:") \
	_( CMGL,		"+CMGL",		"+CMGL:") \
	_( CMS_ERROR,   "+CMS ERROR",   "+CMS ERROR:") \
	_( CMTI,        "+CMTI",        "+CMTI:") \
	_( CMT,			"+CMT",			"+CMT:") \
	_( CDSI,        "+CDSI",        "+CDSI:") \
	_( CDS,			"+CDS",			"+CDS:") \
	_( CBM,			"+CBM",			"+CBM:") \
\
	_( CNUM,        "+CNUM",        "+CNUM:") \
		/* and "ERROR+CNUM:", hacked later on */ \
\
	_( DSCI,        "^DSCI",        "^DSCI:") \
	_( CEND,        "^CEND",        "VOICE CALL:") \
	_( CPIN,        "+CPIN",        "+CPIN:") \
\
	_( CREG,        "+CREG",        "+CREG:") \
	_( CSQ,         "+CSQ",         "+CSQ:") \
	_( CSSI,        "+CSSI",        "+CSSI:") \
	_( CSSU,        "+CSSU",        "+CSSU:") \
\
	_( CUSD,        "+CUSD",        "+CUSD:") \
	_( ERROR,       "ERROR",        "ERROR\r") \
		/* and "COMMAND NOT SUPPORT\r", hacked later on */ \
\
	_( QIND,        "+QIND",        "+QIND:") \
	_( NO_CARRIER,  "NO CARRIER",   "NO CARRIER\r") \
\
	_( NO_DIALTONE, "NO DIALTONE",  "NO DIALTONE\r") \
	_( NO_ANSWER,   "NO ANSWER",    "NO ANSWER\r") \
	_( OK,          "OK",           "OK\r") \
	_( CONF,        "^CONF",        "MISSED_CALL:") \
	_( RING,        "RING",         "RING\r") \
\
	_( SMMEMFULL,   "^SMMEMFULL",   "^SMMEMFULL:") \
	_( SMS_PROMPT,  "> ",           "> ") \
	_( SRVST,       "^SRVST",       "^SRVST:") \
\
	_( CVOICE,      "^CVOICE",      "^CVOICE:") \
	_( CSMS,		"+CSMS",		"+CSMS:") \
	_( CMGS,        "+CMGS",        "+CMGS:") \
	_( CPMS,        "+CPMS",        "+CPMS:") \
	_( CSCA,        "+CSCA",        "+CSCA:") \
\
	_( CLCC,        "+CLCC",        "+CLCC:") \
	_( RCEND,       "CALLEND",      "REMOTE CALL END") \
	_( CCWA,        "+CCWA",        "+CCWA:") \
	_( QSPN,        "+QSPN",        "+QSPN:") \
	_( QNWINFO,		"+QNWINFO",		"+QNWINFO:") \
	_( QTONEDET,	"+QTONEDET",	"+QTONEDET:") \
	_( QAUDLOOP,	"+QAUDLOOP",	"+QAUDLOOP:") \
	_( QAUDMOD,		"+QAUDMOD",		"+QAUDMOD:") \
	_( QPCMV,		"+QPCMV",		"+QPCMV:") \
	_( QLTS,		"+QLTS",		"+QLTS:") \
	_( CCLK,		"+CCLK",		"+CCLK:") \
	_( QMIC,		"+QMIC",		"+QMIC:") \
	_( QRXGAIN, 	"+QRXGAIN",		"+QRXGAIN:") \
	_( CTXVOL, 		"+CTXVOL",		"+CTXVOL:") \
	_( CRXVOL, 		"+CRXVOL",		"+CRXVOL:") \

/* AT_RESPONSES_TABLE */

typedef enum {

	/* Hackish way to force RES_PARSE_ERROR = -1 for compatibility */
	COMPATIBILITY_RES_START_AT_MINUSONE = -2,

	AT_RESPONSES_TABLE(AT_RES_AS_ENUM)

	/* Hackish way to maintain MAX and MIN responses for compatibility */
	RES_MIN = RES_PARSE_ERROR,
	RES_MAX = RES_CRXVOL,
} at_res_t;

/*! response description */
typedef struct at_response_t
{
	at_res_t		res;
	const char*		name;
	const char*		id;
	unsigned		idlen;
} at_response_t;

/*! responses control */
typedef struct at_responses_t
{
	const at_response_t*	responses;
	unsigned		ids_first;		/*!< index of first id */
	unsigned		ids;			/*!< number of ids */
	int			name_first;		/*!< value of enum for first name */
	int			name_last;		/*!< value of enum for last name */
} at_responses_t;

/*! responses description */
extern const at_responses_t at_responses;
const char* at_res2str (at_res_t res);

int at_response(struct pvt* pvt, const struct ast_str* const result, at_res_t at_res);
int at_poll_sms(struct pvt* pvt);

#endif /* CHAN_QUECTEL_AT_RESPONSE_H_INCLUDED */