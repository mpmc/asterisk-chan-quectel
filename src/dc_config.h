/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_QUECTEL_DC_CONFIG_H_INCLUDED
#define CHAN_QUECTEL_DC_CONFIG_H_INCLUDED

#include "ast_config.h"

#include <asterisk/channel.h>		/* AST_MAX_CONTEXT MAX_LANGUAGE */

#include "mutils.h"

#define CONFIG_FILE		"quectel.conf"
#define DEVNAMELEN		31
#define IMEI_SIZE		15
#define IMSI_SIZE		15
#define PATHLEN			256
#define DEVPATHLEN		256

typedef enum {
	TRIBOOL_NONE = 0,
	TRIBOOL_FALSE = -1,
	TRIBOOL_TRUE = 1
} tristate_bool_t;

typedef enum {
	DEV_STATE_STOPPED	= 0,
	DEV_STATE_RESTARTED,
	DEV_STATE_REMOVED,
	DEV_STATE_STARTED,
} dev_state_t;

typedef enum {
	CALL_WAITING_DISALLOWED = 0,
	CALL_WAITING_ALLOWED,
	CALL_WAITING_AUTO
} call_waiting_t;

typedef enum {
	MESSAGE_STORAGE_AUTO = 0,
	MESSAGE_STORAGE_SM,
	MESSAGE_STORAGE_ME,
	MESSAGE_STORAGE_MT,
	MESSAGE_STORAGE_SR,
} message_storage_t;

const char * dc_cw_setting2str(call_waiting_t);

tristate_bool_t dc_str23stbool(const char*);
int dc_str23stbool_ex(const char*, tristate_bool_t*, const char*);

const char* dc_3stbool2str(int);
const char* dc_3stbool2str_ex(int, const char*);
const char* dc_3stbool2str_capitalized(int);

message_storage_t dc_str2msgstor(const char*);
const char* dc_msgstor2str(message_storage_t);

/*
 Config API
 Operations
 	convert from string to native
 	convent from native to string
 	get native value
	get alternative presentation

 	set native value ?

	types:
		string of limited length
		integer with limits
		enum
		boolean
*/

/* Global inherited (shared) settings */
typedef struct dc_sconfig
{
	char			context[AST_MAX_CONTEXT];	/*!< the context for incoming calls; 'default '*/
	char			exten[AST_MAX_EXTENSION];	/*!< exten, not overwrite valid subscriber_number */
	char			language[MAX_LANGUAGE];		/*!< default language 'en' */

	int				group;						/*!< group number for group dialling 0 */
	int				rxgain;						/*!< increase the incoming volume 0 */
	int				txgain;						/*!< increase the outgoint volume 0 */
	int				callingpres;				/*!< calling presentation */

	unsigned int	usecallingpres:1;		/*! -1 */
	unsigned int	autodeletesms:1;		/*! 0 */
	unsigned int	resetquectel:1;			/*! 1 */
	unsigned int	disablesms:1;			/*! 0 */
	unsigned int	multiparty:1;			/*! 0 */
	unsigned int	dtmf:1;					/*! 0 */
	unsigned int	moh:1;					/*! 0 */
	unsigned int	query_time:1;			/*! 0 */

	long				dtmf_duration;		/*! duration of DTMF in miliseconds */
	dev_state_t			initstate;			/*! DEV_STATE_STARTED */
	call_waiting_t		callwaiting;		/*!< enable/disable/auto call waiting CALL_WAITING_AUTO */

	int					msg_service;
	tristate_bool_t		msg_direct;
	message_storage_t 	msg_storage;		/*! MESSAGE_STORAGE_AUTO */
} dc_sconfig_t;

/* Global settings */
typedef struct dc_gconfig
{
	struct ast_jb_conf	jbconf;				/*!< jitter buffer settings, disabled by default */
	int			discovery_interval;		/*!< The device discovery interval */
#define DEFAULT_DISCOVERY_INT	60
	char sms_db[PATHLEN];
#define DEFAULT_SMS_DB ":memory:"
	int csms_ttl;
#define DEFAULT_CSMS_TTL 600

} dc_gconfig_t;

/* Local required (unique) settings */
typedef struct dc_uconfig
{
	/* unique settings */
	char			id[DEVNAMELEN];			/*!< id from quectel.conf */
	char			audio_tty[DEVPATHLEN];	/*!< tty for audio connection */
	char			data_tty[DEVPATHLEN];	/*!< tty for AT commands */
	char			imei[IMEI_SIZE+1];		/*!< search device by imei */
	char			imsi[IMSI_SIZE+1];		/*!< search device by imsi */
    char			alsadev[DEVNAMELEN];	/*!< ALSA audio device name */
	tristate_bool_t	uac;					/*!< handle audio by audio device (UAC) */
	unsigned int	slin16:1;				/*!< SLIN16 audio format */
} dc_uconfig_t;

#define DEFAULT_ALSADEV "hw:Android"
#define DEFAULT_ALSADEV_EXT "hw:0"

/* all Config settings join in one place */
typedef struct pvt_config
{
	dc_uconfig_t		unique;				/*!< unique settings */
	dc_sconfig_t		shared;				/*!< possible inherited settings */
} pvt_config_t;
#define SCONFIG(cfg,name)	((cfg)->shared.name)
#define UCONFIG(cfg,name)	((cfg)->unique.name)

void dc_sconfig_fill_defaults(struct dc_sconfig * config);
void dc_sconfig_fill(struct ast_config * cfg, const char * cat, struct dc_sconfig * config);
void dc_gconfig_fill(struct ast_config * cfg, const char * cat, struct dc_gconfig * config);
int dc_config_fill(struct ast_config * cfg, const char * cat, const struct dc_sconfig * parent, struct pvt_config * config);


#endif /* CHAN_QUECTEL_DC_CONFIG_H_INCLUDED */
