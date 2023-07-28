/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#include "dc_config.h"
#include <asterisk/callerid.h>				/* ast_parse_caller_presentation() */
#include "helpers.h"

const static long DEF_DTMF_DURATION = 120;

static struct ast_jb_conf jbconf_default =
{
	.flags			= 0,
	.max_size		= -1,
	.resync_threshold	= -1,
	.impl			= "",
	.target_extra		= -1,
};

const char * dc_cw_setting2str(call_waiting_t cw)
{
	static const char* const options[] = { "disabled", "allowed", "auto" };
	return enum2str(cw, options, ITEMS_OF(options));
}

tristate_bool_t dc_str23stbool(const char* str)
{
	if (!str) return TRIBOOL_NONE;
	if (!strcasecmp(str, "on") || !strcasecmp(str, "true")) {
		return TRIBOOL_TRUE;
	}
	else if (!strcasecmp(str, "off") || !strcasecmp(str, "false")) {
		return TRIBOOL_FALSE;
	}
	else {
		return TRIBOOL_NONE;
	}
}

int dc_str23stbool_ex(const char* str, tristate_bool_t* res, const char* none_val)
{
	if (!str) return -1;
	if (!res) return -2;
	if (!none_val) return -3;

	if (!strcasecmp(str, "on") || !strcasecmp(str, "true")) {
		*res = TRIBOOL_TRUE;
		return 0;
	}
	else if (!strcasecmp(str, "off") || !strcasecmp(str, "false")) {
		*res = TRIBOOL_FALSE;
		return 0;
	}
	else if (!strcasecmp(str, none_val)) {
		*res = TRIBOOL_NONE;
		return 0;		
	}
	else {
		return 1;
	}
}

static unsigned int int23statebool(int v)
{
	if (!v) {
		return 1;
	}
	else {
		return (v < 0)? 0 : 2;
	}
}

const char* dc_3stbool2str(int v)
{
	static const char* const strs[] = {
		"off",
		"none",
		"on"
	};

	const unsigned b = int23statebool(v);
	return enum2str_def(b, strs, ITEMS_OF(strs), "none");	
}

const char* dc_3stbool2str_ex(int v, const char* none_val)
{
	const char* const strs[] = {
		"off",
		S_OR(none_val, "none"),
		"on"
	};

	const unsigned b = int23statebool(v);
	return enum2str_def(b, strs, ITEMS_OF(strs), S_OR(none_val, "none"));
}

const char* dc_3stbool2str_capitalized(int v)
{
	static const char* const strs[] = {
		"Off",
		"None",
		"On"
	};

	unsigned b = int23statebool(v);
	return enum2str_def(b, strs, ITEMS_OF(strs), "None");
}

static unsigned int parse_on_off(const char* const name, const char* const value, unsigned int defval) {
	if (!value) {
		return defval;
	}
	if (ast_true(value)) {
		return 1u;
	}
	if (ast_false(value)) {
		return 0u;
	}

	ast_log(LOG_ERROR, "Invalid value '%s' for configuration option '%s'\n", value, name);
	return defval;
}

static const char* const msgstor_strs[] = {
	"AUTO",
	"SM",
	"ME",
	"MT",
	"SR"
};

message_storage_t dc_str2msgstor(const char* stor)
{
	const int res = str2enum(stor, msgstor_strs, ITEMS_OF(msgstor_strs));
	if (res < 0) return MESSAGE_STORAGE_AUTO;
	return (message_storage_t)res;
}

const char* dc_msgstor2str(message_storage_t stor)
{
	return enum2str_def(stor, msgstor_strs, ITEMS_OF(msgstor_strs), "AUTO");
}

#/* assume config is zerofill */
static int dc_uconfig_fill(struct ast_config * cfg, const char * cat, struct dc_uconfig * config)
{
	const char * audio_tty;
	const char * data_tty;
	const char * imei;
	const char * imsi;
	const char * slin16_str;
	const char * uac_str;
	tristate_bool_t uac;
	const char * alsadev;
	int slin16;

	audio_tty = ast_variable_retrieve (cfg, cat, "audio");
	data_tty  = ast_variable_retrieve (cfg, cat, "data");
	imei = ast_variable_retrieve (cfg, cat, "imei");
	imsi = ast_variable_retrieve (cfg, cat, "imsi");
    uac_str = ast_variable_retrieve (cfg, cat, "uac");
    alsadev = ast_variable_retrieve (cfg, cat, "alsadev");
	slin16_str = ast_variable_retrieve(cfg, cat, "slin16");

	if(imei && strlen(imei) != IMEI_SIZE) {
		ast_log (LOG_WARNING, "[%s] Ignore invalid IMEI value '%s'\n", cat, imei);
		imei = NULL;
	}
	if(imsi && strlen(imsi) != IMSI_SIZE) {
		ast_log (LOG_WARNING, "[%s] Ignore invalid IMSI value '%s'\n", cat, imsi);
		imsi = NULL;
	}

	if (uac_str) {
		if (dc_str23stbool_ex(uac_str, &uac, "ext")) {
			ast_log(LOG_WARNING, "[%s] Ignore invalid value of UAC mode '%s'\n", cat, uac_str);
			uac = TRIBOOL_FALSE;
		}
	}
	else {
		uac = TRIBOOL_FALSE;
	}

	if (slin16_str) {
		slin16 = parse_on_off("slin16", slin16_str, 0u);
	}
	else {
		slin16 = 0;
	}

	if (!data_tty && !imei && !imsi) {
		ast_log (LOG_ERROR, "Skipping device %s. Missing required data_tty setting\n", cat);
		return 1;
	}

	ast_copy_string (config->id,		cat,	             sizeof (config->id));
	ast_copy_string (config->data_tty,	S_OR(data_tty, ""),  sizeof (config->data_tty));
	ast_copy_string (config->audio_tty,	S_OR(audio_tty, ""), sizeof (config->audio_tty));
	ast_copy_string (config->imei,		S_OR(imei, ""),	     sizeof (config->imei));
	ast_copy_string (config->imsi,		S_OR(imsi, ""),	     sizeof (config->imsi));
	config->uac = uac;
	switch(uac) {
		case TRIBOOL_FALSE:
		ast_copy_string(config->alsadev, S_OR(alsadev, ""), sizeof(config->alsadev));		
		break;

		case TRIBOOL_NONE:
		ast_copy_string(config->alsadev, S_OR(alsadev, DEFAULT_ALSADEV_EXT), sizeof(config->alsadev));
		break;

		case TRIBOOL_TRUE:
		ast_copy_string(config->alsadev, S_OR(alsadev, DEFAULT_ALSADEV), sizeof(config->alsadev));
		break;
	}
	config->slin16 = (unsigned int)slin16;

	return 0;
}

#/* */
void dc_sconfig_fill_defaults(struct dc_sconfig * config)
{
	/* first set default values */
	memset(config, 0, sizeof(*config));

	ast_copy_string(config->context, "default", sizeof(config->context));
	ast_copy_string(config->exten, "", sizeof(config->exten));
	ast_copy_string(config->language, DEFAULT_LANGUAGE, sizeof(config->language));

	config->resetquectel	=  1;
	config->callingpres		= -1;
	config->initstate		= DEV_STATE_STARTED;
	config->callwaiting 	= CALL_WAITING_AUTO;
	config->moh				= 1;
	config->rxgain			= -1;
	config->txgain			= -1;
	config->msg_service		= -1;
	config->dtmf_duration	= DEF_DTMF_DURATION;
	config->qhup			= 1u;
}

#/* */
void dc_sconfig_fill(struct ast_config * cfg, const char * cat, struct dc_sconfig * config)
{
	struct ast_variable * v;

	/*  read config and translate to values */
	for (v = ast_variable_browse(cfg, cat); v; v = v->next) {
		if (!strcasecmp (v->name, "context")) {
			ast_copy_string (config->context, v->value, sizeof (config->context));
		}
		else if (!strcasecmp (v->name, "exten")) {
			ast_copy_string (config->exten, v->value, sizeof (config->exten));
		}
		else if (!strcasecmp (v->name, "language")) {
			ast_copy_string (config->language, v->value, sizeof (config->language));/* set channel language */
		}
		else if (!strcasecmp (v->name, "group")) {
			config->group = (int) strtol (v->value, (char**) NULL, 10);		/* group is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "rxgain")) {
			if (str2gain(v->value, &config->rxgain)) {
				config->rxgain = -1;
			}
		}
		else if (!strcasecmp (v->name, "txgain")) {
			if (str2gain(v->value, &config->txgain)) {
				config->txgain = -1;
			}
		}
		else if (!strcasecmp (v->name, "callingpres")) {
			config->callingpres = ast_parse_caller_presentation (v->value);
			if (config->callingpres == -1)
			{
				errno = 0;
				config->callingpres = (int) strtol (v->value, (char**) NULL, 10);/* callingpres is set to -1 if invalid */
				if (config->callingpres == 0 && errno == EINVAL)
				{
					config->callingpres = -1;
				}
			}
		}
		else if (!strcasecmp (v->name, "usecallingpres")) {
			config->usecallingpres = parse_on_off(v->name, v->value, 0u); /* usecallingpres is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "autodeletesms")) {
			config->autodeletesms = parse_on_off(v->name, v->value, 0u); /* autodeletesms is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "resetquectel")) {
			config->resetquectel = parse_on_off(v->name, v->value, 0u); /* resetquectel is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "disablesms")) {
			config->disablesms = parse_on_off(v->name, v->value, 0u); /* disablesms is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "disable")) {
			const unsigned int is = parse_on_off(v->name, v->value, 0u);
			config->initstate = is ? DEV_STATE_REMOVED : DEV_STATE_STARTED;
		}
		else if (!strcasecmp (v->name, "initstate")) {
			const dev_state_t val = str2dev_state(v->value);
			if(val == DEV_STATE_STOPPED || val == DEV_STATE_STARTED || val == DEV_STATE_REMOVED)
				config->initstate = val;
			else
				ast_log(LOG_ERROR, "Invalid value for 'initstate': '%s', must be one of 'stop' 'start' 'remove' default is 'start'\n", v->value);
		}
		else if (!strcasecmp (v->name, "callwaiting")) {
			if(strcasecmp(v->value, "auto"))
				config->callwaiting = parse_on_off(v->name, v->value, 0u);
		}
		else if (!strcasecmp (v->name, "multiparty")) {
			config->multiparty = parse_on_off(v->name, v->value, 0u);
		}
		else if (!strcasecmp (v->name, "dtmf")) {
			config->dtmf = parse_on_off(v->name, v->value, 0u);
		}
		else if (!strcasecmp (v->name, "dtmf_duration")) {
			config->dtmf_duration = strtol(v->value, (char**) NULL, 10);
			if (config->dtmf_duration <= 0 && errno == EINVAL) {
				config->dtmf_duration = DEF_DTMF_DURATION;
			}
		}		
		else if (!strcasecmp (v->name, "moh")) {
			config->moh = parse_on_off(v->name, v->value, 0u);
		}
		else if (!strcasecmp (v->name, "query_time")) {
			config->query_time = parse_on_off(v->name, v->value, 0u);
		}
		else if (!strcasecmp (v->name, "dsci")) {
			config->dsci = parse_on_off(v->name, v->value, 0u);
		}
		else if (!strcasecmp (v->name, "qhup")) {
			config->qhup = parse_on_off(v->name, v->value, 1u);
		}		
		else if (!strcasecmp (v->name, "msg_direct")) {
			config->msg_direct = dc_str23stbool(v->value);
		}
		else if (!strcasecmp (v->name, "msg_storage")) {
			config->msg_storage = dc_str2msgstor(v->value);
		}
		else if (!strcasecmp (v->name, "msg_service")) {
			config->msg_service = (int)strtol(v->value, (char**)NULL, 10);
		}
	}
}

#/* */
void dc_gconfig_fill(struct ast_config * cfg, const char * cat, struct dc_gconfig * config)
{
	/* set default values */
	memcpy(&config->jbconf, &jbconf_default, sizeof(config->jbconf));
	config->discovery_interval = DEFAULT_DISCOVERY_INT;
	ast_copy_string(config->sms_db, DEFAULT_SMS_DB, sizeof(config->sms_db));
	config->csms_ttl = DEFAULT_CSMS_TTL;

	const char* const stmp = ast_variable_retrieve(cfg, cat, "interval");
	if (stmp) {
		errno = 0;
		const int tmp = (int) strtol (stmp, (char**) NULL, 10);
		if (tmp == 0 && errno == EINVAL)
			ast_log(LOG_NOTICE, "Error parsing 'interval' in general section, using default value %d\n", config->discovery_interval);
		else
			config->discovery_interval = tmp;
	}

	const char* const smsdb = ast_variable_retrieve(cfg, cat, "smsdb");
	if (smsdb) {
		ast_copy_string(config->sms_db, smsdb, sizeof(config->sms_db));
	}

	const char* const csmsttl = ast_variable_retrieve(cfg, cat, "csmsttl");
	if (csmsttl) {
		errno = 0;
		const long tmp = strtol(csmsttl, (char**) NULL, 10);
		if (tmp == 0 && errno == EINVAL)
			ast_log(LOG_NOTICE, "Error parsing 'csmsttl' in general section, using default value %d\n", config->csms_ttl);
		else
			config->csms_ttl = tmp;
	}

	for (const struct ast_variable* v = ast_variable_browse(cfg, cat); v; v = v->next)
		/* handle jb conf */
		ast_jb_read_conf(&config->jbconf, v->name, v->value);
}

#/* */
int dc_config_fill(struct ast_config * cfg, const char * cat, const struct dc_sconfig * parent, struct pvt_config * config)
{
	/* try set unique first */
	int err = dc_uconfig_fill(cfg, cat, &config->unique);
	if(!err)
	{
		/* inherit from parent */
		memcpy(&config->shared, parent, sizeof(config->shared));

		/* overwrite local */
		dc_sconfig_fill(cfg, cat, &config->shared);
	}

	return err;
}
