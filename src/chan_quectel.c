/*
 * chan_quectel
 *
 * Copyright (C) 2011-2015
 * bg <bg_one@mail.ru>
 * http://www.e1550.mobi
 *
 * chan_quectel is based on chan_datacard by
 *
 * Artem Makhutov <artem@makhutov.org>
 * http://www.makhutov.org
 *
 * Dmitry Vagin <dmitry2004@yandex.ru>
 *
 * chan_datacard is based on chan_mobile by Digium
 * (Mark Spencer <markster@digium.com>)
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief UMTS Voice Quectel channel driver
 *
 * \author Artem Makhutov <artem@makhutov.org>
 * \author Dave Bowerman <david.bowerman@gmail.com>
 * \author Dmitry Vagin <dmitry2004@yandex.ru>
 * \author bg <bg_one@mail.ru>
 * \author Max von Buelow <max@m9x.de>
 *
 * \ingroup channel_drivers
 */
#include "ast_config.h"

#if ASTERISK_VERSION_NUM < 140000 /* 14- */
ASTERISK_FILE_VERSION(__FILE__, "$Rev: " PACKAGE_REVISION " $")
#endif /* 14- */

#include <asterisk/stringfields.h>	/* AST_DECLARE_STRING_FIELDS for asterisk/manager.h */
#include <asterisk/manager.h>
#include <asterisk/callerid.h>
#include <asterisk/module.h>		/* AST_MODULE_LOAD_DECLINE ... */
#include <asterisk/timing.h>		/* ast_timer_open() ast_timer_fd() */
#include <asterisk/causes.h>

#include <sys/stat.h>			/* S_IRUSR | S_IRGRP | S_IROTH */
#include <termios.h>			/* struct termios tcgetattr() tcsetattr()  */
#include <pthread.h>			/* pthread_t pthread_kill() pthread_join() */
#include <fcntl.h>			/* O_RDWR O_NOCTTY */
#include <signal.h>			/* SIGURG */

#include "ast_compat.h"			/* asterisk compatibility fixes */

#if ASTERISK_VERSION_NUM >= 130000 /* 13+ */
#include <asterisk/stasis_channels.h>
#include <asterisk/format_cache.h>
#endif /* ^13+ */

#include "chan_quectel.h"
#include "at_response.h"		/* at_res_t */
#include "at_queue.h"			/* struct at_queue_task_cmd at_queue_head_cmd() */
#include "at_command.h"			/* at_cmd2str() */
#include "mutils.h"			/* ITEMS_OF() */
#include "at_read.h"
#include "cli.h"
#include "channel.h"			/* channel_queue_hangup() */
#include "dc_config.h"			/* dc_uconfig_fill() dc_gconfig_fill() dc_sconfig_fill()  */
#include "pdiscovery.h"			/* pdiscovery_lookup() pdiscovery_init() pdiscovery_fini() */
#include "smsdb.h"
#include "error.h"
#include "errno.h"

const char * const dev_state_strs[4] = { "stop", "restart", "remove", "start" };
public_state_t * gpublic;
#if ASTERISK_VERSION_NUM >= 100000 && ASTERISK_VERSION_NUM < 130000 /* 10-13 */
struct ast_format chan_quectel_format;
struct ast_format_cap * chan_quectel_format_cap;
#endif /* ^10-13 */

static snd_pcm_t *alsa_card_init(struct pvt* pvt, const char *dev, snd_pcm_stream_t stream)
{
	int res;
	int direction;
	snd_pcm_t *handle = NULL;
	snd_pcm_hw_params_t *hwparams = NULL;
	snd_pcm_sw_params_t *swparams = NULL;
	snd_pcm_uframes_t period_size = PERIOD_FRAMES * 4;
	snd_pcm_uframes_t buffer_size = PERIOD_FRAMES * 8;
	snd_pcm_uframes_t boundary = 0u;
	unsigned int rate = DESIRED_RATE;
	snd_pcm_uframes_t start_threshold;

	const char* const stream_str = (stream == SND_PCM_STREAM_CAPTURE)? "CAPTURE" : "PLAYBACK";

	res = snd_pcm_open(&handle, dev, stream, SND_PCM_NONBLOCK);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] Fail to open device - dev:'%s' err:'%s'\n", PVT_ID(pvt), stream_str, dev, snd_strerror(res));
		return NULL;
	} else {
		ast_debug(1, "[%s][ALSA][%s] Device opened - dev:'%s'\n", PVT_ID(pvt), stream_str, dev);
	}

	hwparams = ast_alloca(snd_pcm_hw_params_sizeof());
	memset(hwparams, 0, snd_pcm_hw_params_sizeof());
	snd_pcm_hw_params_any(handle, hwparams);

	res = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] HW Set access failed: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
	}

	res = snd_pcm_hw_params_set_format(handle, hwparams, format);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] HW Set format failed: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
	}

	res = snd_pcm_hw_params_set_channels(handle, hwparams, 1);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] HW Set channels failed: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
	}

	direction = 0;
	res = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, &direction);
	if (rate != DESIRED_RATE) {
		ast_log(LOG_WARNING, "[%s][ALSA][%s] HW Rate not correct -  requested:%d got:%u\n", PVT_ID(pvt), stream_str, DESIRED_RATE, rate);
	}

	direction = 0;
	res = snd_pcm_hw_params_set_period_size_near(handle, hwparams, &period_size, &direction);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] HW Period size (%lu frames) is bad: %s\n", PVT_ID(pvt), stream_str, period_size, snd_strerror(res));
	}

	res = snd_pcm_hw_params_set_buffer_size_near(handle, hwparams, &buffer_size);
	if (res < 0) {
		ast_log(LOG_WARNING, "[%s][ALSA][%s] HW Problem setting buffer size of %lu: %s\n", PVT_ID(pvt), stream_str, buffer_size, snd_strerror(res));
	}

	res = snd_pcm_hw_params(handle, hwparams);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] Couldn't set the new HW params: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
	}

	res = snd_pcm_hw_params_current(handle, hwparams);
	if (res < 0) {
		ast_log(LOG_WARNING, "[%s][ALSA][%s] HW Couldn't get current HW params: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
	}
	else {
		res = snd_pcm_hw_params_get_period_size(hwparams, &period_size, NULL);
		if (res >= 0) ast_debug(1, "[%s][ALSA][%s] Period size: %lu\n", PVT_ID(pvt), stream_str, period_size);

		res = snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
		if (res >= 0) ast_debug(1, "[%s][ALSA][%s] Buffer size: %lu\n", PVT_ID(pvt), stream_str, buffer_size);
	}

	swparams = ast_alloca(snd_pcm_sw_params_sizeof());
	memset(swparams, 0, snd_pcm_sw_params_sizeof());
	snd_pcm_sw_params_current(handle, swparams);

	res = snd_pcm_sw_params_get_boundary(swparams, &boundary);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] SW Couldn't get boundary: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
		boundary = 0u;
	}
	ast_debug(2, "[%s][ALSA][%s] Boundary: %lu\n", PVT_ID(pvt), stream_str, boundary);

	if (stream == SND_PCM_STREAM_PLAYBACK)
		start_threshold = period_size;
	else
		start_threshold = 1;

	res = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] SW Couldn't set start threshold: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
	}

	if (boundary > 0u) {
		res = snd_pcm_sw_params_set_stop_threshold(handle, swparams, boundary);
		if (res < 0) {
			ast_log(LOG_ERROR, "[%s][ALSA][%s] SW Couldn't set stop threshold: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
		}
	}

	if (stream == SND_PCM_STREAM_PLAYBACK && boundary > 0u) {
		res = snd_pcm_sw_params_set_silence_threshold(handle, swparams, 0);
		if (res < 0) {
			ast_log(LOG_ERROR, "[%s][ALSA][%s] SW Couldn't set silence threshold: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
		}
		else {
			res = snd_pcm_sw_params_set_silence_size(handle, swparams, boundary);
			if (res < 0) {
				ast_log(LOG_ERROR, "[%s][ALSA][%s] SW Couldn't set silence size: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
			}
		}	
	}

	res = snd_pcm_sw_params(handle, swparams);
	if (res < 0) {
		ast_log(LOG_ERROR, "[%s][ALSA][%s] Couldn't set SW params: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
	}

	if (stream == SND_PCM_STREAM_CAPTURE) {
		res = snd_pcm_poll_descriptors_count(handle);
		if (res <= 0) {
			ast_log(LOG_ERROR, "[%s][ALSA][%s] Unable to get a poll descriptors count: %s\n", PVT_ID(pvt), stream_str, snd_strerror(res));
		}
		else if (res != 1) {
			ast_debug(1, "[%s][ALSA][%s] Can't handle more than one FD - cnt:%d\n", PVT_ID(pvt), stream_str, res);
		}
		else {
			struct pollfd pfd;

			snd_pcm_poll_descriptors(handle, &pfd, res);
			ast_debug(1, "[%s][ALSA][%s] Acquired FD:%d from the poll descriptor\n", PVT_ID(pvt), stream_str, pfd.fd);
			pvt->audio_fd = pfd.fd;
		}
	}

	return handle;
}

static int soundcard_init(struct pvt * pvt)
{
	pvt->icard = alsa_card_init(pvt, CONF_UNIQ(pvt, alsadev), SND_PCM_STREAM_CAPTURE);
	if (!pvt->icard) {
		ast_log(LOG_ERROR, "[%s][ALSA] Problem opening capture device '%s'\n", PVT_ID(pvt), CONF_UNIQ(pvt, alsadev));
		return -1;
	}

	pvt->ocard = alsa_card_init(pvt, CONF_UNIQ(pvt, alsadev), SND_PCM_STREAM_PLAYBACK);
	if (!pvt->ocard) {
		ast_log(LOG_ERROR, "[%s][ALSA] Problem opening playback device '%s'\n", PVT_ID(pvt), CONF_UNIQ(pvt, alsadev));
		return -1;
	}

	int err = snd_pcm_link(pvt->icard, pvt->ocard);
	if (err < 0) {
		ast_log(LOG_WARNING, "[%s][ALSA] Couldn't link devices: %s\n", PVT_ID(pvt), snd_strerror(err));
	}

	ast_verb(2, "[%s] Sound card '%s' initialized\n", PVT_ID(pvt), CONF_UNIQ(pvt, alsadev));
	return 0;
}

static int public_state_init(struct public_state * state);


/*!
 * Get status of the quectel. It might happen that the device disappears
 * (e.g. due to a USB unplug).
 *
 * \return 0 if device seems ok, non-0 if it seems not available
 */

static int port_status (int fd)
{
	struct termios t;

	if (fd < 0)
	{
		return -1;
	}

	return tcgetattr (fd, &t);
}

#/* return length of lockname */
static int lock_build(const char * devname, char * buf, unsigned length)
{
	const char * basename;
	char resolved_path[PATH_MAX];

	/* follow symlinks */
	if(realpath(devname, resolved_path) != NULL)
		devname = resolved_path;

	basename = strrchr(devname, '/');
	if(basename)
		basename++;
	else
		basename = devname;

	/* NOTE: use system system wide lock directory */
	#if defined(__FreeBSD__)
	return snprintf(buf, length, "/var/spool/lock/LCK..%s", basename);
	#else
	return snprintf(buf, length, "/var/lock/LCK..%s", basename);
	#endif
}

#/* return 0 on error */
static int lock_create(const char * lockfile)
{
	int fd;
	int len = 0;
	char pidb[21];

	fd = open(lockfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IRGRP | S_IROTH);
	if(fd >= 0)
	{
		/* NOTE: bg: i assume next open reuse same fd - not thread-safe */
		len = snprintf(pidb, sizeof(pidb), "%d %d", getpid(), fd);
		len = write(fd, pidb, len);
		close(fd);
	} else {
		ast_log(LOG_ERROR, "open('%s') failed: %s\n", lockfile, strerror(errno));
	}

	return len;
}

#/* return pid of owner, 0 if free */
int lock_try(const char * devname, char ** lockname)
{
	int fd;
	int len;
	int pid = 0;
	int assigned;
	int fd2;
	char name[1024];
	char buffer[65];

	lock_build(devname, name, sizeof(name));

	/* FIXME: rise conditions: some time between lock check and got lock */
	fd = open(name, O_RDONLY);
	if(fd >= 0)
	{
		len = read(fd, buffer, sizeof(buffer) - 1);
		if(len > 0)
		{
			buffer[len] = 0;
			assigned = sscanf(buffer, "%d %d", &len, &fd2);
			if(assigned > 0 && kill(len, 0) == 0)
			{
				if(len == getpid() && assigned > 1)
				{
					if(port_status(fd2) == 0)
						pid = len;
				}
				else
					pid = len;
			}
		}
		close(fd);
	}

	if(pid == 0)
	{
		unlink(name);
		lock_create(name);
		*lockname = ast_strdup(name);
	}
	return pid;
}

#/* */
void closetty(int fd, char ** lockfname)
{
	close(fd);

	/* remove lock */
	unlink(*lockfname);
	ast_free(*lockfname);
	*lockfname = NULL;
}

int opentty(const char* dev, char ** lockfile, int typ)
{
	int flags;
	int pid;
	int fd;
	struct termios term_attr;
	char buf[40];

	pid = lock_try(dev, lockfile);
	if(pid != 0) {
		ast_log(LOG_WARNING, "%s already used by process %d\n", dev, pid);
		return -1;
	}

	fd = open(dev, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		flags = errno;
		closetty(fd, lockfile);
		snprintf(buf, sizeof(buf), "Open Failed\r\nErrorCode: %d", flags);
		ast_log(LOG_WARNING, "unable to open %s: %s\n", dev, strerror(flags));
		return -1;
	}

	/* Put the terminal into exclusive mode. All other open(2)s by
	 * non-root will fail with EBUSY. */
	if (ioctl(fd, TIOCEXCL) != 0) {
		ast_log(LOG_WARNING, "ioctl(TIOCEXCL) failed for %s: %s\n", dev, strerror(errno));
	}

	flags = fcntl(fd, F_GETFD);
	if (flags == -1 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		flags = errno;
		closetty(fd, lockfile);
		ast_log(LOG_WARNING, "fcntl(F_GETFD/F_SETFD) failed for %s: %s\n", dev, strerror(flags));
		return -1;
	}

	if (tcgetattr(fd, &term_attr) != 0) {
		flags = errno;
		closetty(fd, lockfile);
		ast_log (LOG_WARNING, "tcgetattr() failed for %s: %s\n", dev, strerror(flags));
		return -1;
	}

	if (typ == 1)
		term_attr.c_cflag = B115200 | CS8 | CREAD | CRTSCTS | CLOCAL;
	else
		term_attr.c_cflag = B115200 | CS8 | CREAD | CRTSCTS;

	term_attr.c_iflag = 0;
	term_attr.c_oflag = 0;
	term_attr.c_lflag = 0;
	term_attr.c_cc[VMIN] = 1;
	term_attr.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSAFLUSH, &term_attr) != 0) {
		ast_log (LOG_WARNING, "tcsetattr(TCSAFLUSH) failed for %s: %s\n", dev, strerror(errno));
	}

	return fd;
}

static int pcm_close(snd_pcm_t** ad, snd_pcm_stream_t stream_type)
{
	if (*ad == NULL) return 0;
	const int res = snd_pcm_close(*ad);
	if (res < 0) {
		switch (stream_type) {
			case SND_PCM_STREAM_PLAYBACK:
				ast_log(LOG_ERROR, "ALSA - failed to close playback device: %s", snd_strerror(res));
				break;

			case SND_PCM_STREAM_CAPTURE:
				ast_log(LOG_ERROR, "ALSA - failed to close capture device: %s", snd_strerror(res));
				break;
		}
	}
	*ad = NULL;
	return res;
}

#/* phone monitor thread pvt cleanup */
static void disconnect_quectel(struct pvt* pvt)
{
	if (!PVT_NO_CHANS(pvt)) {
		struct cpvt* cpvt;
		AST_LIST_TRAVERSE(&(pvt->chans), cpvt, entry) {
			at_hangup_immediately(cpvt, AST_CAUSE_NORMAL_UNSPECIFIED);
			CPVT_RESET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
			change_channel_state(cpvt, CALL_STATE_RELEASED, AST_CAUSE_NORMAL_UNSPECIFIED);
		}
	}

	if (CONF_UNIQ(pvt, uac)) {
		at_disable_uac_immediately(pvt);
	}

	at_queue_run_immediately(pvt);
	at_queue_flush(pvt);

    if (CONF_UNIQ(pvt, uac)) {
		const int err = snd_pcm_unlink(pvt->icard);
		if (err < 0) {
			ast_log(LOG_WARNING, "[%s][ALSA] Couldn't unlink devices: %s", PVT_ID(pvt), snd_strerror(err));
		}
		if (pvt->icard) pcm_close(&pvt->icard, SND_PCM_STREAM_CAPTURE);
		if (pvt->ocard)	pcm_close(&pvt->ocard, SND_PCM_STREAM_PLAYBACK);
    }
	else {
		closetty(pvt->audio_fd, &pvt->alock);
	}

	closetty(pvt->data_fd, &pvt->dlock);

	pvt->data_fd = -1;
	pvt->audio_fd = -1;

	pvt_on_remove_last_channel(pvt);

	ast_debug(1, "[%s] Quectel disconnecting - cleaning up\n", PVT_ID(pvt));

	/* unaffected in case of restart */
	pvt->use_ucs2_encoding = 0;
	pvt->gsm_reg_status = -1;
	pvt->rssi = 0;
	pvt->act = 0;
	pvt->operator = 0;
	
	ast_string_field_set(pvt, manufacturer, NULL);
	ast_string_field_set(pvt, model, NULL);
	ast_string_field_set(pvt, firmware, NULL);
	ast_string_field_set(pvt, imei, NULL);
	ast_string_field_set(pvt, imsi, NULL);
	ast_string_field_set(pvt, location_area_code, NULL);
	ast_string_field_set(pvt, network_name, NULL);
	ast_string_field_set(pvt, short_network_name, NULL);
	ast_string_field_set(pvt, provider_name, "NONE");
	ast_string_field_set(pvt, band, NULL);
	ast_string_field_set(pvt, cell_id, NULL);
	ast_string_field_set(pvt, sms_scenter, NULL);
	ast_string_field_set(pvt, subscriber_number, "Unknown");
	ast_string_field_set(pvt, module_time, NULL);

	pvt->has_subscriber_number = 0;

	pvt->gsm_registered	= 0;
	pvt->has_sms = 0;
	pvt->has_voice = 0;
	pvt->has_call_waiting = 0;

	pvt->connected		= 0;
	pvt->initialized	= 0;
	pvt->has_call_waiting	= 0;

	/* FIXME: LOST real device state */
	pvt->dialing = 0;
	pvt->ring = 0;
	pvt->cwaiting = 0;
	pvt->outgoing_sms = 0;
	pvt->incoming_sms_index = -1;
	pvt->volume_sync_step = VOLUME_SYNC_BEGIN;

	pvt->current_state = DEV_STATE_STOPPED;

	/* clear statictics */
	memset(&pvt->stat, 0, sizeof(pvt->stat));

	ast_copy_string(PVT_STATE(pvt, data_tty),  CONF_UNIQ(pvt, data_tty), sizeof (PVT_STATE(pvt, data_tty)));
	ast_copy_string(PVT_STATE(pvt, audio_tty), CONF_UNIQ(pvt, audio_tty), sizeof (PVT_STATE(pvt, audio_tty)));

	ast_verb(3, "[%s] Quectel has disconnected\n", PVT_ID(pvt));
}

/* anybody wrote some to device before me, and not read results, clean pending results here */
#/* */
void clean_read_data(const char* devname, int fd, struct ringbuffer* const rb)
{
	rb_reset(rb);
	for (int t = 0; at_wait(fd, &t); t = 0) {
		const int iovcnt = at_read(fd, devname, rb);
		if (iovcnt) ast_debug(4, "[%s] Drop %u bytes of pending data\n", devname, (unsigned)rb_used(rb));
		/* drop read */
		rb_reset(rb);
		if (!iovcnt) break;
	}
}

static void handle_expired_reports(struct pvt *pvt)
{
	char dst[SMSDB_DST_MAX_LEN];
	char payload[SMSDB_PAYLOAD_MAX_LEN];
	ssize_t payload_len = smsdb_outgoing_purge_one(dst, payload);
	if (payload_len >= 0) {
		ast_verb (3, "[%s] TTL payload: %.*s\n", PVT_ID(pvt), (int) payload_len, payload);
		const channel_var_t vars[] =
		{
			{ "SMS_REPORT_PAYLOAD", payload },
			{ "SMS_REPORT_TS", "" },
			{ "SMS_REPORT_DT", "" },
			{ "SMS_REPORT_SUCCESS", "0" },
			{ "SMS_REPORT_TYPE", "t" },
			{ "SMS_REPORT", "" },
			{ NULL, NULL },
		};
		start_local_channel(pvt, "report", dst, vars);
	}
}

/*!
 * \brief Check if the module is unloading.
 * \retval 0 not unloading
 * \retval 1 unloading
 */

static void* do_monitor_phone(void* data)
{
	static const size_t RINGBUFFER_SIZE = 2 * 1024;

	struct pvt*	const pvt = (struct pvt*)data;
	char dev[sizeof(PVT_ID(pvt))];

	struct ringbuffer rb;

	void* const buf = ast_malloc(RINGBUFFER_SIZE);
	struct ast_str* const result = ast_str_create(RINGBUFFER_SIZE);
	rb_init(&rb, buf, RINGBUFFER_SIZE);

	pvt->timeout = DATA_READ_TIMEOUT;
	ast_mutex_lock(&pvt->lock);

	/* 4 reduce locking time make copy of this readonly fields */
	const int fd = pvt->data_fd;
	ast_copy_string(dev, PVT_ID(pvt), sizeof(dev));

	clean_read_data(dev, fd, &rb);

	/* schedule quectel initilization  */
	if (at_enqueue_initialization(&pvt->sys_chan, CMD_AT)) {
		ast_log(LOG_ERROR, "[%s] Error adding initialization commands to queue\n", dev);
		goto e_cleanup;
	}

	/* Poll first SMS, if any */
	if (at_poll_sms(pvt) == 0) {
		ast_debug(1, "[%s] Polling first SMS message\n", PVT_ID(pvt));
	}

	ast_mutex_unlock(&pvt->lock);

	while (1) {
		ast_mutex_lock(&pvt->lock);

		handle_expired_reports(pvt);
		if (port_status(pvt->data_fd)) {
			ast_log(LOG_ERROR, "[%s] Lost connection to Quectel\n", dev);
			goto e_cleanup;
		}

		if (!CONF_UNIQ(pvt, uac)) {
			if (port_status(pvt->audio_fd)) {
				ast_log(LOG_ERROR, "[%s] Lost connection to Quectel\n", dev);
				goto e_cleanup;
			}
		}

		if(pvt->terminate_monitor) {
			ast_log(LOG_NOTICE, "[%s] Stopping by %s request\n", dev, dev_state2str(pvt->desired_state));
			goto e_restart;
		}

		int t = at_queue_timeout(pvt);
		if(t < 0) t = pvt->timeout;

		ast_mutex_unlock(&pvt->lock);

		if (!at_wait(fd, &t)) {
			ast_mutex_lock(&pvt->lock);
			const struct at_queue_cmd* const ecmd = at_queue_head_cmd(pvt);
			if(ecmd) {
				ast_log(LOG_ERROR, "[%s][%s] Timeout [%s]\n", dev, at_cmd2str(ecmd->cmd), at_res2str(ecmd->res));
				goto e_cleanup;
			}
			at_enqueue_ping(&pvt->sys_chan);
			ast_mutex_unlock (&pvt->lock);
			continue;
		}

		/* FIXME: access to device not locked */
		int iovcnt = at_read(fd, dev, &rb);
		if (iovcnt < 0) {
			break;
		}

		ast_mutex_lock (&pvt->lock);
		PVT_STAT(pvt, d_read_bytes) += iovcnt;
		ast_mutex_unlock (&pvt->lock);

		struct iovec iov[2];
		int	read_result = 0;
		size_t skip = 0u;

		while ((iovcnt = at_read_result_iov(dev, &read_result, &skip, &rb, iov, result)) > 0) {
			const size_t len = at_combine_iov(result, iov, iovcnt);
			const at_res_t at_res = at_str2res(result);
			if (at_res != RES_UNKNOWN) ast_str_trim_blanks(result);
			rb_read_upd(&rb, len + skip);
			skip = 0u;

			ast_mutex_lock(&pvt->lock);
			PVT_STAT(pvt, at_responses) ++;
			if (at_response(pvt, result, at_res)) {
				ast_log(LOG_ERROR, "[%s] Fail to handle response\n", dev);
				goto e_cleanup;
			}
			ast_mutex_unlock(&pvt->lock);
		}

		ast_mutex_lock(&pvt->lock);
		if (!pvt->terminate_monitor) {
			if (at_queue_run(pvt)) {
				ast_log(LOG_ERROR, "[%s] Fail to run command from queue\n", dev);
				goto e_cleanup;
			}
		}
		ast_mutex_unlock(&pvt->lock);
	}
	ast_mutex_lock (&pvt->lock);

e_cleanup:
	if (!pvt->initialized) {
		// TODO: send monitor event
		ast_verb(3, "[%s] Error initializing channel\n", dev);
	}
	/* it real, unsolicited disconnect */
	pvt->terminate_monitor = 0;

e_restart:
	disconnect_quectel(pvt);
//	pvt->monitor_running = 0;
	ast_mutex_unlock(&pvt->lock);

	ast_free(buf);
	ast_free(result);

	/* TODO: wakeup discovery thread after some delay */
	return NULL;
}

static int start_monitor(struct pvt * pvt)
{
	if (ast_pthread_create_background (&pvt->monitor_thread, NULL, do_monitor_phone, pvt) < 0) {
		pvt->monitor_thread = AST_PTHREADT_NULL;
		return 0;
	}

	return 1;
}

#/* */
static void pvt_stop(struct pvt * pvt)
{
	pthread_t id;

	if(pvt->monitor_thread != AST_PTHREADT_NULL)
	{
		pvt->terminate_monitor = 1;
		pthread_kill (pvt->monitor_thread, SIGURG);
		id = pvt->monitor_thread;

		ast_mutex_unlock (&pvt->lock);
		pthread_join (id, NULL);
		ast_mutex_lock (&pvt->lock);

		pvt->terminate_monitor = 0;
		pvt->monitor_thread = AST_PTHREADT_NULL;
	}
//	pvt->current_state = DEV_STATE_STOPPED;
}

#/* called with pvt lock hold */
static int pvt_discovery(struct pvt * pvt)
{
	char devname[DEVNAMELEN];
	char imei[IMEI_SIZE+1];
	char imsi[IMSI_SIZE+1];

	int resolved;
	if(CONF_UNIQ(pvt, data_tty)[0] == 0 && CONF_UNIQ(pvt, audio_tty)[0] == 0) {
		char * data_tty;
		char * audio_tty;

		ast_copy_string(devname, PVT_ID(pvt), sizeof(devname));
		ast_copy_string(imei, CONF_UNIQ(pvt, imei), sizeof(imei));
		ast_copy_string(imsi, CONF_UNIQ(pvt, imsi), sizeof(imsi));

		ast_debug(3, "[%s] Trying ports discovery for%s%s%s%s\n",
			PVT_ID(pvt),
			imei[0] == 0 ? "" : " IMEI ",
			imei,
			imsi[0] == 0 ? "" : " IMSI ",
			imsi
			);
		ast_mutex_unlock (&pvt->lock);
//sleep(10); // test
		resolved = pdiscovery_lookup(devname, imei, imsi, &data_tty, &audio_tty);
		ast_mutex_lock (&pvt->lock);

		if(resolved) {
			ast_copy_string (PVT_STATE(pvt, data_tty),  data_tty,  sizeof (PVT_STATE(pvt, data_tty)));
			ast_copy_string (PVT_STATE(pvt, audio_tty), audio_tty, sizeof (PVT_STATE(pvt, audio_tty)));

			ast_free(audio_tty);
			ast_free(data_tty);
			ast_verb (3, "[%s]%s%s%s%s found on data_tty=%s audio_tty=%s\n",
				PVT_ID(pvt),
				imei[0] == 0 ? "" : " IMEI ",
				imei,
				imsi[0] == 0 ? "" : " IMSI ",
				imsi,
				PVT_STATE(pvt, data_tty),
				PVT_STATE(pvt, audio_tty)
				);
		} else {
			ast_debug(3, "[%s] Not found ports for%s%s%s%s\n",
				PVT_ID(pvt),
				imei[0] == 0 ? "" : " IMEI ",
				imei,
				imsi[0] == 0 ? "" : " IMSI ",
				imsi
				);
		}
	} else {
		ast_copy_string (PVT_STATE(pvt, data_tty),  CONF_UNIQ(pvt, data_tty), sizeof (PVT_STATE(pvt, data_tty)));
		ast_copy_string (PVT_STATE(pvt, audio_tty), CONF_UNIQ(pvt, audio_tty), sizeof (PVT_STATE(pvt, audio_tty)));
		resolved = 1;
	}
	return ! resolved;
}

#/* */
static void pvt_start(struct pvt * pvt)
{
	long flags;

	/* prevent start_monitor() multiple times and on turned off devices */
	if (pvt->connected || pvt->desired_state != DEV_STATE_STARTED) {
		// || (pvt->monitor_thread != AST_PTHREADT_NULL &&
		//     (pthread_kill(pvt->monitor_thread, 0) == 0 || errno != ESRCH))
		return;
	}

	pvt_stop(pvt);

	if (pvt_discovery(pvt)) {
		return;
	}

	ast_verb(3, "[%s] Trying to connect on %s...\n", PVT_ID(pvt), PVT_STATE(pvt, data_tty));

	pvt->data_fd = opentty(PVT_STATE(pvt, data_tty), &pvt->dlock, 0);
	if (pvt->data_fd < 0) {
		return;
	}
	if (CONF_UNIQ(pvt, uac)) {
		if (soundcard_init(pvt) < 0) {
			disconnect_quectel(pvt);
			goto cleanup_datafd;
		}
	}
	else {
		// TODO: delay until device activate voice call or at pvt_on_create_1st_channel()
		pvt->audio_fd = opentty(PVT_STATE(pvt, audio_tty), &pvt->alock, pvt->is_simcom);
		if (pvt->audio_fd < 0) {
			goto cleanup_datafd;
		}
    }

	if (!start_monitor(pvt)) {
		if (CONF_UNIQ(pvt, uac))
			goto cleanup_datafd;
		else
			goto cleanup_audiofd;
	}

	/* Set data_fd and audio_fd to non-blocking. This appears to fix
	 * incidental deadlocks occurring with Asterisk 12+ or with
	 * jitterbuffer enabled. Apparently Asterisk can call the
	 * (audio) read function for sockets that don't have data to
	 * read(). */
	flags = fcntl(pvt->data_fd, F_GETFL);
	fcntl(pvt->data_fd, F_SETFL, flags | O_NONBLOCK);
	if (!CONF_UNIQ(pvt, uac)) {
		flags = fcntl(pvt->audio_fd, F_GETFL);
		fcntl(pvt->audio_fd, F_SETFL, flags | O_NONBLOCK);
	}

	pvt->connected = 1;
	pvt->current_state = DEV_STATE_STARTED;
	ast_verb(3, "[%s] Quectel has connected, initializing...\n", PVT_ID(pvt));
	return;

cleanup_audiofd:
	if (pvt->audio_fd > 0) closetty(pvt->audio_fd, &pvt->alock);

cleanup_datafd:
	closetty(pvt->data_fd, &pvt->dlock);
}

#/* */
static void pvt_free(struct pvt * pvt)
{
	at_queue_flush(pvt);

	ast_string_field_free_memory(pvt);

	ast_mutex_unlock(&pvt->lock);
	ast_free(pvt);
}

#/* */
static void pvt_destroy(struct pvt * pvt)
{
	ast_mutex_lock(&pvt->lock);
	pvt_stop(pvt);
	pvt_free(pvt);
}

static void * do_discovery(void * arg)
{
	struct public_state * state = (struct public_state *) arg;
	struct pvt * pvt;

	while(state->unloading_flag == 0)
	{
		/* read lock for avoid deadlock when IMEI/IMSI discovery */
		AST_RWLIST_RDLOCK(&state->devices);
		AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			pvt->must_remove = 0;

			if(pvt->restart_time == RESTATE_TIME_NOW && pvt->desired_state != pvt->current_state)
			{
				switch(pvt->desired_state)
				{
					case DEV_STATE_RESTARTED:
						pvt_stop(pvt);
						pvt->desired_state = DEV_STATE_STARTED;
						/* fall through */
					case DEV_STATE_STARTED:
						pvt_start(pvt);
						break;
					case DEV_STATE_REMOVED:
						pvt_stop(pvt);
						pvt->must_remove = 1;
						break;
					case DEV_STATE_STOPPED:
						pvt_stop(pvt);
				}
			}
			ast_mutex_unlock (&pvt->lock);
		}
		AST_RWLIST_UNLOCK (&state->devices);

		/* actual device removal here for avoid long (discovery) time write lock on device list in loop above */
		AST_RWLIST_WRLOCK(&state->devices);
		AST_RWLIST_TRAVERSE_SAFE_BEGIN(&state->devices, pvt, entry)
		{
			ast_mutex_lock(&pvt->lock);
			if(pvt->must_remove)
			{
				AST_RWLIST_REMOVE_CURRENT(entry);
				pvt_free(pvt);
			} else
				ast_mutex_unlock(&pvt->lock);
		}
		AST_RWLIST_TRAVERSE_SAFE_END;
		AST_RWLIST_UNLOCK(&state->devices);

		/* Go to sleep (only if we are not unloading) */
		if (state->unloading_flag == 0)
		{
			sleep(SCONF_GLOBAL(state, discovery_interval));
		}
	}

	return NULL;
}

#/* */
static int discovery_restart(public_state_t * state)
{
	if(state->discovery_thread == AST_PTHREADT_STOP)
		return 0;

	ast_mutex_lock(&state->discovery_lock);
	if (state->discovery_thread == pthread_self()) {
		ast_mutex_unlock(&state->discovery_lock);
		ast_log(LOG_WARNING, "Cannot kill myself\n");
		return -1;
	}
	if (state->discovery_thread != AST_PTHREADT_NULL) {
		/* Wake up the thread */
		pthread_kill(state->discovery_thread, SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create_background(&state->discovery_thread, NULL, do_discovery, state) < 0) {
			ast_mutex_unlock(&state->discovery_lock);
			ast_log(LOG_ERROR, "Unable to start discovery thread\n");
			return -1;
		}
	}
	ast_mutex_unlock(&state->discovery_lock);
	return 0;
}

#/* */
static void discovery_stop(public_state_t * state)
{
	/* signal for discovery unloading */
	state->unloading_flag = 1;

	ast_mutex_lock(&state->discovery_lock);
	if (state->discovery_thread && (state->discovery_thread != AST_PTHREADT_STOP) && (state->discovery_thread != AST_PTHREADT_NULL)) {
//		pthread_cancel(state->discovery_thread);
		pthread_kill(state->discovery_thread, SIGURG);
		pthread_join(state->discovery_thread, NULL);
	}

	state->discovery_thread = AST_PTHREADT_STOP;
	ast_mutex_unlock(&state->discovery_lock);
}

#/* */
void pvt_on_create_1st_channel(struct pvt* pvt)
{
	if (CONF_SHARED(pvt, multiparty)) {
		if (CONF_UNIQ(pvt, uac)) {
			ast_log(LOG_ERROR, "[%s] Multiparty mode not supported in UAC mode\n", PVT_ID(pvt));
		}
		else {
			mixb_init(&pvt->a_write_mixb, pvt->a_write_buf, sizeof(pvt->a_write_buf));
			// rb_init (&pvt->a_write_rb, pvt->a_write_buf, sizeof (pvt->a_write_buf));
			if(!pvt->a_timer) pvt->a_timer = ast_timer_open();
		}
	}
}

#/* */
void pvt_on_remove_last_channel(struct pvt* pvt)
{
	if (pvt->a_timer) {
		ast_timer_close(pvt->a_timer);
		pvt->a_timer = NULL;
	}
}

#define SET_BIT(dw_array,bitno)		do { (dw_array)[(bitno) >> 5] |= 1 << ((bitno) & 31) ; } while(0)
#define TEST_BIT(dw_array,bitno)	((dw_array)[(bitno) >> 5] & 1 << ((bitno) & 31))
#/* */
int pvt_get_pseudo_call_idx(const struct pvt * pvt)
{
	struct cpvt * cpvt;
	int * bits;
	int dwords = ((MAX_CALL_IDX + sizeof(*bits) - 1) / sizeof(*bits));

	bits = alloca(dwords * sizeof(*bits));
	memset(bits, 0, dwords * sizeof(*bits));

	AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
		SET_BIT(bits, cpvt->call_idx);
	}

	for(dwords = 1; dwords <= MAX_CALL_IDX; dwords++)
	{
		if(!TEST_BIT(bits, dwords))
			return dwords;
	}
	return 0;
}

#undef TEST_BIT
#undef SET_BIT

#/* */
static int is_dial_possible2(const struct pvt * pvt, int opts, const struct cpvt * ignore_cpvt)
{
	struct cpvt * cpvt;
	int hold = 0;
	int active = 0;
	// FIXME: allow HOLD states for CONFERENCE
	int use_call_waiting = opts & CALL_FLAG_HOLD_OTHER;

	if(pvt->ring || pvt->cwaiting || pvt->dialing)
		return 0;

	AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
		switch(cpvt->state)
		{
			case CALL_STATE_INIT:
				if(cpvt != ignore_cpvt)
					return 0;
				break;

			case CALL_STATE_DIALING:
			case CALL_STATE_ALERTING:
			case CALL_STATE_INCOMING:
			case CALL_STATE_WAITING:
				return 0;

			case CALL_STATE_ACTIVE:
				if(hold || !use_call_waiting)
					return 0;
				active++;
				break;

			case CALL_STATE_ONHOLD:
				if(active || !use_call_waiting)
					return 0;
				hold++;
				break;

			case CALL_STATE_RELEASED:
				;
		}
	}
	return 1;
}

#/* */
int is_dial_possible(const struct pvt * pvt, int opts)
{
	return is_dial_possible2(pvt, opts, NULL);
}

#/* */
int pvt_enabled(const struct pvt * pvt)
{
	return pvt->current_state == DEV_STATE_STARTED && (pvt->desired_state == pvt->current_state || pvt->restart_time == RESTATE_TIME_CONVENIENT);
}

#/* */
int ready4voice_call(const struct pvt* pvt, const struct cpvt * current_cpvt, int opts)
{
	if(!pvt->connected
		|| !pvt->initialized
		|| !pvt->has_voice
		|| !pvt->gsm_registered
		|| !pvt_enabled(pvt)) {
		return 0;
	}

	return is_dial_possible2(pvt, opts, current_cpvt);
}


#/* */
static int can_dial(struct pvt* pvt, int opts, const struct ast_channel * requestor)
{
	/* not allow hold requester channel :) */
	/* FIXME: requestor may be just proxy/masquerade for real channel */
	//	use ast_bridged_channel(chan) ?
	//	use requestor->tech->get_base_channel() ?

	if((opts & CALL_FLAG_HOLD_OTHER) == CALL_FLAG_HOLD_OTHER && channels_loop(pvt, requestor))
		return 0;
	return ready4voice_call(pvt, NULL, opts);
}

#/* return locked pvt or NULL */
struct pvt * find_device_ex(struct public_state * state, const char * name)
{
	struct pvt * pvt;

	AST_RWLIST_RDLOCK(&state->devices);
	AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
	{
		ast_mutex_lock (&pvt->lock);
		if (!strcmp (PVT_ID(pvt), name))
		{
			break;
		}
		ast_mutex_unlock (&pvt->lock);
	}
	AST_RWLIST_UNLOCK(&state->devices);

	return pvt;
}

#/* return locked pvt or NULL */
struct pvt * find_device_ext (const char * name)
{
	struct pvt * pvt = find_device(name);

	if (pvt) {
		if (!pvt_enabled(pvt)) {
			ast_mutex_unlock (&pvt->lock);
			chan_quectel_err = E_DEVICE_DISABLED;
			pvt = NULL;
		}
	} else {
		chan_quectel_err = E_DEVICE_NOT_FOUND;
	}
	return pvt;
}

#/* like find_device but for resource spec; return locked! pvt or NULL */
struct pvt * find_device_by_resource_ex(struct public_state * state, const char * resource, int opts, const struct ast_channel * requestor, int * exists)
{
	int group;
	size_t i;
	size_t j;
	size_t c;
	size_t last_used;
	struct pvt * pvt;
	struct pvt * found = NULL;
	struct pvt * round_robin[MAXQUECTELDEVICES];

	*exists = 0;
	/* Find requested device and make sure it's connected and initialized. */
	AST_RWLIST_RDLOCK(&state->devices);

	if (((resource[0] == 'g') || (resource[0] == 'G')) && ((resource[1] >= '0') && (resource[1] <= '9')))
	{
		errno = 0;
		group = (int) strtol (&resource[1], (char**) NULL, 10);
		if (errno != EINVAL)
		{
			AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
			{
				ast_mutex_lock (&pvt->lock);

				if (CONF_SHARED(pvt, group) == group)
				{
					*exists = 1;
					if(can_dial(pvt, opts, requestor))
					{
						found = pvt;
						break;
					}
				}
				ast_mutex_unlock (&pvt->lock);
			}
		}
	}
	else if (((resource[0] == 'r') || (resource[0] == 'R')) && ((resource[1] >= '0') && (resource[1] <= '9')))
	{
		errno = 0;
		group = (int) strtol (&resource[1], (char**) NULL, 10);
		if (errno != EINVAL)
		{
			/* Generate a list of all available devices */
			j = ITEMS_OF (round_robin);
			c = 0; last_used = 0;
			AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
			{
				ast_mutex_lock (&pvt->lock);
				if (CONF_SHARED(pvt, group) == group)
				{
					round_robin[c] = pvt;
					if (pvt->group_last_used == 1)
					{
						pvt->group_last_used = 0;
						last_used = c;
					}

					c++;

					if (c == j)
					{
						ast_mutex_unlock (&pvt->lock);
						break;
					}
				}
				ast_mutex_unlock (&pvt->lock);
			}

			/* Search for a available device starting at the last used device */
			for (i = 0, j = last_used + 1; i < c; i++, j++)
			{
				if (j == c)
				{
					j = 0;
				}

				pvt = round_robin[j];
				*exists = 1;

				ast_mutex_lock (&pvt->lock);
				if (can_dial(pvt, opts, requestor))
				{
					pvt->group_last_used = 1;
					found = pvt;
					break;
				}
				ast_mutex_unlock (&pvt->lock);
			}
		}
	}
	else if (((resource[0] == 'p') || (resource[0] == 'P')) && resource[1] == ':')
	{
		/* Generate a list of all available devices */
		j = ITEMS_OF(round_robin);
		c = 0; last_used = 0;
		AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			if (!strcmp (pvt->provider_name, &resource[2]))
			{
				round_robin[c] = pvt;
				if (pvt->prov_last_used == 1)
				{
					pvt->prov_last_used = 0;
					last_used = c;
				}

				c++;

				if (c == j)
				{
					ast_mutex_unlock (&pvt->lock);
					break;
				}
			}
			ast_mutex_unlock (&pvt->lock);
		}

		/* Search for a available device starting at the last used device */
		for (i = 0, j = last_used + 1; i < c; i++, j++)
		{
			if (j == c)
			{
				j = 0;
			}

			pvt = round_robin[j];
			*exists = 1;

			ast_mutex_lock (&pvt->lock);
			if (can_dial(pvt, opts, requestor))
			{
				pvt->prov_last_used = 1;
				found = pvt;
				break;
			}
			ast_mutex_unlock (&pvt->lock);
		}
	}
	else if (((resource[0] == 's') || (resource[0] == 'S')) && resource[1] == ':')
	{
		/* Generate a list of all available devices */
		j = ITEMS_OF(round_robin);
		c = 0; last_used = 0;
		i = strlen (&resource[2]);

		AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			if (!strncmp (pvt->imsi, &resource[2], i))
			{
				round_robin[c] = pvt;
				if (pvt->sim_last_used == 1)
				{
					pvt->sim_last_used = 0;
					last_used = c;
				}

				c++;

				if (c == j)
				{
					ast_mutex_unlock (&pvt->lock);
					break;
				}
			}
			ast_mutex_unlock (&pvt->lock);
		}

		/* Search for a available device starting at the last used device */
		for (i = 0, j = last_used + 1; i < c; i++, j++)
		{
			if (j == c)
			{
				j = 0;
			}

			pvt = round_robin[j];
			*exists = 1;

			ast_mutex_lock (&pvt->lock);
			if (can_dial(pvt, opts, requestor))
			{
				pvt->sim_last_used = 1;
				found = pvt;
				break;
			}
			ast_mutex_unlock (&pvt->lock);
		}
	}
	else if (((resource[0] == 'i') || (resource[0] == 'I')) && resource[1] == ':')
	{
		AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			if (!strcmp(pvt->imei, &resource[2]))
			{
				*exists = 1;
				if(can_dial(pvt, opts, requestor))
				{
					found = pvt;
					break;
				}
			}
			ast_mutex_unlock (&pvt->lock);
		}
	}
	else
	{
		AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			if (!strcmp (PVT_ID(pvt), resource))
			{
				*exists = 1;
				if(can_dial(pvt, opts, requestor))
				{
					found = pvt;
					break;
				}
			}
			ast_mutex_unlock (&pvt->lock);
		}
	}

	AST_RWLIST_UNLOCK(&state->devices);
	return found;
}

#/* */
static const char * pvt_state_base(const struct pvt * pvt)
{
	const char * state = NULL;
// length is "AAAAAAAAAA"
	if(pvt->current_state == DEV_STATE_STOPPED && pvt->desired_state == DEV_STATE_STOPPED)
		state = "Stopped";
	else if(!pvt->connected)
		state = "Not connected";
	else if(!pvt->initialized)
		state = "Not initialized";
	else if(!pvt->gsm_registered)
		state = "GSM not registered";
	return state;
}


#/* */
const char* pvt_str_state(const struct pvt* pvt)
{
	const char * state = pvt_state_base(pvt);
	if(!state) {
		if(pvt->ring || PVT_STATE(pvt, chan_count[CALL_STATE_INCOMING]))
			state = "Ring";
		else if(pvt->cwaiting || PVT_STATE(pvt, chan_count[CALL_STATE_WAITING]))
			state = "Waiting";
		else if(pvt->dialing ||
			(PVT_STATE(pvt, chan_count[CALL_STATE_INIT])
				+
				PVT_STATE(pvt, chan_count[CALL_STATE_DIALING])
				+
				PVT_STATE(pvt, chan_count[CALL_STATE_ALERTING])) > 0)
			state = "Dialing";

		else if(PVT_STATE(pvt, chan_count[CALL_STATE_ACTIVE]) > 0)
//			state = "Active";
			state = pvt_call_dir(pvt);
		else if(PVT_STATE(pvt, chan_count[CALL_STATE_ONHOLD]) > 0)
			state = "Held";
		else if(pvt->outgoing_sms || pvt->incoming_sms_index >= 0)
			state = "SMS";
		else
			state = "Free";
	}
	return state;
}

#/* */
struct ast_str* pvt_str_state_ex(const struct pvt* pvt)
{
	struct ast_str* buf = ast_str_create (256);
	const char * state = pvt_state_base(pvt);

	if(state)
		ast_str_append (&buf, 0, "%s", state);
	else
	{
		if(pvt->ring || PVT_STATE(pvt, chan_count[CALL_STATE_INCOMING]))
			ast_str_append (&buf, 0, "Ring ");

		if(pvt->dialing ||
			(PVT_STATE(pvt, chan_count[CALL_STATE_INIT])
				+
			PVT_STATE(pvt, chan_count[CALL_STATE_DIALING])
				+
			PVT_STATE(pvt, chan_count[CALL_STATE_ALERTING])) > 0)
			ast_str_append (&buf, 0, "Dialing ");

		if(pvt->cwaiting || PVT_STATE(pvt, chan_count[CALL_STATE_WAITING]))
			ast_str_append (&buf, 0, "Waiting ");

		if(PVT_STATE(pvt, chan_count[CALL_STATE_ACTIVE]) > 0)
			ast_str_append (&buf, 0, "Active %u ", PVT_STATE(pvt, chan_count[CALL_STATE_ACTIVE]));

		if(PVT_STATE(pvt, chan_count[CALL_STATE_ONHOLD]) > 0)
			ast_str_append (&buf, 0, "Held %u ", PVT_STATE(pvt, chan_count[CALL_STATE_ONHOLD]));

		if(pvt->incoming_sms_index >= 0)
			ast_str_append (&buf, 0, "Incoming SMS ");

		if(pvt->outgoing_sms)
			ast_str_append (&buf, 0, "Outgoing SMS");

		if(ast_str_strlen(buf) == 0)
		{
			ast_str_append (&buf, 0, "%s", "Free");
		}
	}

	if(pvt->desired_state != pvt->current_state)
		ast_str_append (&buf, 0, " %s", dev_state2str_msg(pvt->desired_state));

	return buf;
}

#/* */
const char* GSM_regstate2str(int gsm_reg_status)
{
	static const char * const gsm_states[] = {
		"Not registered, not searching",
		"Registered, home network",
		"Not registered, but searching",
		"Registration denied",
		"Unknown",
		"Registered, roaming",
		};
	return enum2str_def(gsm_reg_status, gsm_states, ITEMS_OF (gsm_states), "Unknown");
}

#/* */
const char * sys_act2str(int act)
{
	static const char * const sys_acts[] = {
		"No service",
		"GSM",
		"GPRS",
		"EDGE",
		"WCDMA",
		"HSDPA",
		"HSUPA",
		"HSDPA and HSUPA",
		"LTE",
		"TDS-CDMA",
		"TDS-HSDPA only",
		"TDS-HSUPA only",
		"TDS-HSPA (HSDPA and HSUPA)",
		"CDMA",
		"EVDO",
		"CDMA and EVDO",
		"CDMA and LTE",
		"???",
		"???",
		"???",
		"???",
		"???",
		"???",
		"Ehrpd",
		"CDMA and Ehrpd",
	};

	return enum2str_def(act, sys_acts, ITEMS_OF (sys_acts), "Unknown");
}

#/* BUGFIX of https://code.google.com/p/asterisk-chan-quectel/issues/detail?id=118 */
const char* rssi2dBm(int rssi, char* buf, size_t len)
{
	if(rssi <= 0) {
		snprintf(buf, len, "<= -113 dBm");
	}
	else if(rssi <= 30) {
		snprintf(buf, len, "%d dBm", 2 * rssi - 113);
	}
	else if(rssi == 31) {
		snprintf(buf, len, ">= -51 dBm");
	}
	else {
		snprintf(buf, len, "unknown or unmeasurable");
	}
	return buf;
}

/* Module */

static struct pvt * pvt_create(const pvt_config_t * settings)
{
	struct pvt * pvt = ast_calloc (1, sizeof (*pvt));
	if(pvt)
	{
		ast_mutex_init (&pvt->lock);

		AST_LIST_HEAD_INIT_NOLOCK (&pvt->at_queue);
		AST_LIST_HEAD_INIT_NOLOCK (&pvt->chans);
		pvt->sys_chan.pvt = pvt;
		pvt->sys_chan.state = CALL_STATE_RELEASED;

		pvt->monitor_thread		= AST_PTHREADT_NULL;
		pvt->audio_fd			= -1;
		pvt->icard				= NULL;
		pvt->ocard				= NULL;
		pvt->data_fd			= -1;
		pvt->timeout			= DATA_READ_TIMEOUT;
		pvt->gsm_reg_status		= -1;
		pvt->incoming_sms_index	= -1;

		ast_string_field_init(pvt, 14);

		ast_string_field_set(pvt, provider_name, "NONE");
		ast_string_field_set(pvt, subscriber_number, "Unknown");
		
		pvt->has_subscriber_number = 0;
		pvt->desired_state = SCONFIG(settings, initstate);

		/* and copy settings */
		memcpy(&pvt->settings, settings, sizeof(pvt->settings));
		return pvt;
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Skipping device: Error allocating memory\n", UCONFIG(settings, id));
	}
	return NULL;
}

#/* */
static int pvt_time4restate(const struct pvt * pvt)
{
	if(pvt->desired_state != pvt->current_state)
	{
		if(pvt->restart_time == RESTATE_TIME_NOW || (PVT_NO_CHANS(pvt) && !pvt->outgoing_sms && pvt->incoming_sms_index >= 0))
			return 1;
	}
	return 0;
}

#/* */
void pvt_try_restate(struct pvt * pvt)
{
	if(pvt_time4restate(pvt))
	{
		pvt->restart_time = RESTATE_TIME_NOW;
		discovery_restart(gpublic);
	}
}

#/* assume caller hold lock */
static int pvt_reconfigure(struct pvt * pvt, const pvt_config_t * settings, restate_time_t when)
{
	int rv = 0;

	if(SCONFIG(settings, initstate) == DEV_STATE_REMOVED)
	{
		/* handle later, in one place */
		pvt->must_remove = 1;
	}
	else
	{
		/* check what changes require starting or stopping */
		if(pvt->desired_state != SCONFIG(settings, initstate)) {
			pvt->desired_state = SCONFIG(settings, initstate);

			rv = pvt_time4restate(pvt);
			pvt->restart_time = rv ? RESTATE_TIME_NOW : when;
		}

		/* check what config changes require restaring */
		else if(strcmp(UCONFIG(settings, audio_tty), CONF_UNIQ(pvt, audio_tty))
			|| strcmp(UCONFIG(settings, data_tty), CONF_UNIQ(pvt, data_tty))
			|| strcmp(UCONFIG(settings, imei), CONF_UNIQ(pvt, imei))
			|| strcmp(UCONFIG(settings, imsi), CONF_UNIQ(pvt, imsi))
			|| SCONFIG(settings, resetquectel) != CONF_SHARED(pvt, resetquectel)
			|| SCONFIG(settings, callwaiting) != CONF_SHARED(pvt, callwaiting))
		{
			/* TODO: schedule restart */
			pvt->desired_state = DEV_STATE_RESTARTED;

			rv = pvt_time4restate(pvt);
			pvt->restart_time = rv ? RESTATE_TIME_NOW : when;
		}

		/* and copy settings */
		memcpy(&pvt->settings, settings, sizeof(pvt->settings));
	}
	return rv;
}

int pvt_set_act(struct pvt* pvt, int act)
{
	if (pvt->act == act) return act;

	pvt->act = act;
	pvt->rssi = 0;
	ast_string_field_set(pvt, band, NULL);
	return act;
}

#/* */
static int reload_config(public_state_t * state, int recofigure, restate_time_t when, unsigned * reload_immediality)
{
	struct ast_config * cfg;
	const char * cat;
	struct ast_flags config_flags = { 0 };
	struct dc_sconfig config_defaults;
	pvt_config_t settings;
	int err;
	struct pvt * pvt;
	unsigned reload_now = 0;

	if ((cfg = ast_config_load (CONFIG_FILE, config_flags)) == NULL)
	{
		return -1;
	}

	/* read global config */
	dc_gconfig_fill(cfg, "general", &state->global_settings);

	/* read defaults */
	dc_sconfig_fill_defaults(&config_defaults);
	dc_sconfig_fill(cfg, "defaults", &config_defaults);

	/* FIXME: deadlock avoid ? */
	AST_RWLIST_RDLOCK(&state->devices);
	AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
	{
		ast_mutex_lock(&pvt->lock);
		pvt->must_remove = 1;
		ast_mutex_unlock(&pvt->lock);
	}
	AST_RWLIST_UNLOCK(&state->devices);

	/* now load devices */
	for (cat = ast_category_browse (cfg, NULL); cat; cat = ast_category_browse (cfg, cat))
	{
		if (strcasecmp (cat, "general") && strcasecmp (cat, "defaults"))
		{
			err = dc_config_fill(cfg, cat, &config_defaults, &settings);
			if(!err)
			{
				pvt = find_device(UCONFIG(&settings, id));
				if(pvt)
				{
					if(!recofigure)
					{
						ast_log (LOG_ERROR, "device %s already exists, duplicate in config file\n", cat);
					}
					else
					{
						pvt->must_remove = 0;
						reload_now += pvt_reconfigure(pvt, &settings, when);
					}
					ast_mutex_unlock(&pvt->lock);
				}
				else
				{
					/* new device */
					if(SCONFIG(&settings, initstate) == DEV_STATE_REMOVED)
					{
						ast_log (LOG_NOTICE, "Skipping device %s as disabled\n", cat);
					}
					else
					{
						pvt = pvt_create(&settings);
						if(pvt)
						{
							/* FIXME: deadlock avoid ? */
							AST_RWLIST_WRLOCK(&state->devices);
							AST_RWLIST_INSERT_TAIL(&state->devices, pvt, entry);
							AST_RWLIST_UNLOCK(&state->devices);
							reload_now++;

							ast_log (LOG_NOTICE, "[%s] Loaded device\n", PVT_ID(pvt));
						}
					}
				}
			}
		}
	}
	ast_config_destroy (cfg);

	/* FIXME: deadlock avoid ? */
	/* schedule removal of devices not listed in config file or disabled */
	AST_RWLIST_RDLOCK(&state->devices);
	AST_RWLIST_TRAVERSE(&state->devices, pvt, entry)
	{
		ast_mutex_lock(&pvt->lock);
		if(pvt->must_remove)
		{
			pvt->desired_state = DEV_STATE_REMOVED;
			if(pvt_time4restate(pvt))
			{
				pvt->restart_time = RESTATE_TIME_NOW;
				reload_now++;
			}
			else
				pvt->restart_time = when;
		}
		ast_mutex_unlock(&pvt->lock);
	}
	AST_RWLIST_UNLOCK (&state->devices);

	if(reload_immediality)
		*reload_immediality = reload_now;
	return 0;
}


#/* */
static void devices_destroy(public_state_t * state)
{
	struct pvt * pvt;

	/* Destroy the device list */
	AST_RWLIST_WRLOCK(&state->devices);
	while((pvt = AST_RWLIST_REMOVE_HEAD(&state->devices, entry)))
	{
		pvt_destroy(pvt);
	}
	AST_RWLIST_UNLOCK(&state->devices);
}


static int load_module()
{
	int rv;

	gpublic = ast_calloc(1, sizeof(*gpublic));
	if(gpublic)
	{
		pdiscovery_init();
		rv = public_state_init(gpublic);
		if(rv != AST_MODULE_LOAD_SUCCESS)
			ast_free(gpublic);
	}
	else
	{
		ast_log (LOG_ERROR, "Unable to allocate global state structure\n");
		rv = AST_MODULE_LOAD_DECLINE;
	}
	return rv;
}

#/* */
static int public_state_init(struct public_state * state)
{
	int rv = AST_MODULE_LOAD_DECLINE;

	AST_RWLIST_HEAD_INIT(&state->devices);
	ast_mutex_init(&state->discovery_lock);

	state->discovery_thread = AST_PTHREADT_NULL;

	if(reload_config(state, 0, RESTATE_TIME_NOW, NULL) == 0)
	{
		rv = AST_MODULE_LOAD_FAILURE;
		if(discovery_restart(state) == 0)
		{

			/* set preferred capabilities */
#if ASTERISK_VERSION_NUM >= 130000 /* 13+ */
			if (!(channel_tech.capabilities = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT))) {
				return AST_MODULE_LOAD_FAILURE;
			}
			ast_format_cap_append(channel_tech.capabilities, ast_format_slin, 0);
#elif ASTERISK_VERSION_NUM >= 100000 /* 10-13 */
			ast_format_set(&chan_quectel_format, AST_FORMAT_SLINEAR, 0);
# if ASTERISK_VERSION_NUM >= 120000 /* 12+ */
			if (!(channel_tech.capabilities = ast_format_cap_alloc(0))) {
				return AST_MODULE_LOAD_FAILURE;
			}
# else
			if (!(channel_tech.capabilities = ast_format_cap_alloc())) {
				return AST_MODULE_LOAD_FAILURE;
			}
# endif
			ast_format_cap_add(channel_tech.capabilities, &chan_quectel_format);
			chan_quectel_format_cap = channel_tech.capabilities;
#endif /* ^10-13 */

			/* register our channel type */
			if(ast_channel_register(&channel_tech) == 0)
			{
				smsdb_init();
				cli_register();

				return AST_MODULE_LOAD_SUCCESS;
			}
			else
			{
#if ASTERISK_VERSION_NUM >= 130000 /* 13+ */
				ao2_cleanup(channel_tech.capabilities);
				channel_tech.capabilities = NULL;
#elif ASTERISK_VERSION_NUM >= 100000 /* 10-13 */
				channel_tech.capabilities = ast_format_cap_destroy(channel_tech.capabilities);
#endif /* ^10-13 */
				ast_log (LOG_ERROR, "Unable to register channel class %s\n", channel_tech.type);
			}
			discovery_stop(state);
		}
		else
		{
			ast_log (LOG_ERROR, "Unable to create discovery thread\n");
		}
		devices_destroy(state);
	}
	else
	{
		ast_log (LOG_ERROR, "Errors reading config file " CONFIG_FILE ", Not loading module\n");
	}

	ast_mutex_destroy(&state->discovery_lock);
	AST_RWLIST_HEAD_DESTROY(&state->devices);

	return rv;
}

#/* */
static void public_state_fini(struct public_state * state)
{
	/* First, take us out of the channel loop */
	ast_channel_unregister (&channel_tech);
#if ASTERISK_VERSION_NUM >= 130000 /* 13+ */
	ao2_cleanup(channel_tech.capabilities);
	channel_tech.capabilities = NULL;
#elif ASTERISK_VERSION_NUM >= 100000 /* 10-13 */
	channel_tech.capabilities = ast_format_cap_destroy(channel_tech.capabilities);
#endif /* ^10-13 */

	/* Unregister the CLI */
	cli_unregister();

	discovery_stop(state);
	devices_destroy(state);

	ast_mutex_destroy(&state->discovery_lock);
	AST_RWLIST_HEAD_DESTROY(&state->devices);
}

static int unload_module()
{
	public_state_fini(gpublic);
	pdiscovery_fini();
	ast_free(gpublic);
	smsdb_atexit();
	gpublic = NULL;
	return 0;
}


#/* */
void pvt_reload(restate_time_t when)
{
	unsigned dev_reload = 0;
	reload_config(gpublic, 1, when, &dev_reload);
	if(dev_reload > 0)
		discovery_restart(gpublic);
}

#/* */
static int reload_module()
{
	pvt_reload(RESTATE_TIME_GRACEFULLY);
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, MODULE_DESCRIPTION,
#if ASTERISK_VERSION_NUM >= 130000 /* 13+ */
		.support_level = AST_MODULE_SUPPORT_EXTENDED,
#endif /* ^13+ */
		.load = load_module,
		.unload = unload_module,
		.reload = reload_module,
#if ASTERISK_VERSION_NUM >= 130000 /* 13+ */
		.load_pri = AST_MODPRI_CHANNEL_DRIVER,
#endif /* ^13+ */
	       );

//AST_MODULE_INFO_STANDARD (ASTERISK_GPL_KEY, MODULE_DESCRIPTION);

struct ast_module* self_module(void)
{
	return ast_module_info->self;
}
