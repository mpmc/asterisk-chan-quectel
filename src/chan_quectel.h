/*
   Copyright (C) 2009-2015

   bg <bg_one@mail.ru>
   http://www.e1550.mobi

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>
*/
#ifndef CHAN_QUECTEL_H_INCLUDED
#define CHAN_QUECTEL_H_INCLUDED

#include "ast_config.h"

#include <asterisk/lock.h>
#include <asterisk/linkedlists.h>

#include "ast_compat.h"				/* asterisk compatibility fixes */

#include "mixbuffer.h"				/* struct mixbuffer */
//#include "ringbuffer.h"				/* struct ringbuffer */
#include "cpvt.h"				/* struct cpvt */
#include "dc_config.h"				/* pvt_config_t */
#include "at_command.h"

#include <alsa/asoundlib.h>
#define DESIRED_RATE 8000
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
static const snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
#else
static const snd_pcm_format_t format = SND_PCM_FORMAT_S16_BE;
#endif
#define MAX_BUFFER_SIZE 100
#define MODULE_DESCRIPTION	"Channel Driver for Mobile Telephony"
#define MAXQUECTELDEVICES	128

const char* dev_state2str(dev_state_t state);
dev_state_t str2dev_state(const char*);
const char* dev_state2str_msg(dev_state_t state);

#if ASTERISK_VERSION_NUM >= 100000 && ASTERISK_VERSION_NUM < 130000 /* 10-13 */
/* Only linear is allowed */
struct ast_format chan_quectel_format;
struct ast_format_cap * chan_quectel_format_cap;
#endif /* ^10-13 */

typedef enum {
	RESTATE_TIME_NOW	= 0,
	RESTATE_TIME_GRACEFULLY,
	RESTATE_TIME_CONVENIENT,
} restate_time_t;

/* state */
typedef struct pvt_state
{
	char			audio_tty[DEVPATHLEN];		/*!< tty for audio connection */
	char			data_tty[DEVPATHLEN];		/*!< tty for AT commands */

	uint32_t		at_tasks;			/*!< number of active tasks in at_queue */
	uint32_t		at_cmds;			/*!< number of active commands in at_queue */
	uint32_t		chansno;			/*!< number of channels in channels list */
	uint8_t			chan_count[CALL_STATES_NUMBER];	/*!< channel number grouped by state */
} pvt_state_t;

#define PVT_STATE_T(state, name)			((state)->name)

/* statictics */
typedef struct pvt_stat
{
	uint32_t		at_tasks;			/*!< number of tasks added to queue */
	uint32_t		at_cmds;			/*!< number of commands added to queue */
	uint32_t		at_responses;			/*!< number of responses handled */

	uint32_t		d_read_bytes;			/*!< number of bytes of commands actually read from device */
	uint32_t		d_write_bytes;			/*!< number of bytes of commands actually written to device */

	uint64_t		a_read_bytes;			/*!< number of bytes of audio read from device */
	uint64_t		a_write_bytes;			/*!< number of bytes of audio written to device */

	uint32_t		read_frames;			/*!< number of frames read from device */
	uint32_t		read_sframes;			/*!< number of truncated frames read from device */

	uint32_t		write_frames;			/*!< number of tries to frame write */
	uint32_t		write_tframes;			/*!< number of truncated frames to write */
	uint32_t		write_sframes;			/*!< number of silence frames to write */

	uint64_t		write_rb_overflow_bytes;	/*!< number of overflow bytes */
	uint32_t		write_rb_overflow;		/*!< number of times when a_write_rb overflowed */

	uint32_t		in_calls;			/*!< number of incoming calls not including waiting */
	uint32_t		cw_calls;			/*!< number of waiting calls */
	uint32_t		out_calls;			/*!< number of all outgoing calls attempts */
	uint32_t		in_calls_handled;		/*!< number of ncoming/waiting calls passed to dialplan */
	uint32_t		in_pbx_fails;			/*!< number of start_pbx fails */

	uint32_t		calls_answered[2];		/*!< number of outgoing and incoming/waiting calls answered */
	uint32_t		calls_duration[2];		/*!< seconds of outgoing and incoming/waiting calls */
} pvt_stat_t;

#define PVT_STAT_T(stat, name)			((stat)->name)

struct at_queue_task;

typedef struct pvt
{
	AST_LIST_ENTRY (pvt)	entry;				/*!< linked list pointers */

	ast_mutex_t		lock;				/*!< pvt lock */
	AST_LIST_HEAD_NOLOCK (, at_queue_task) at_queue;	/*!< queue for commands to modem */

	AST_LIST_HEAD_NOLOCK (, cpvt)		chans;		/*!< list of channels */
	struct cpvt		sys_chan;			/*!< system channel */

	pthread_t		monitor_thread;			/*!< monitor (at commands reader) thread handle */

	int			audio_fd;			/*!< audio descriptor */
    snd_pcm_t 	*icard;
	snd_pcm_t 	*ocard;
	unsigned int ocard_channels;
		
	int			data_fd;			/*!< data descriptor */
#ifdef USE_SYSV_UUCP_LOCKS	
	char		* alock;			/*!< name of lockfile for audio */
	char		* dlock;			/*!< name of lockfile for data */
#endif	

	struct ast_timer*	a_timer;			/*!< audio write timer */

	char			a_silence_buf[FRAME_SIZE_PLAYBACK * 2];

	char			a_write_buf[FRAME_SIZE_PLAYBACK * 5];	/*!< audio write buffer */
	struct mixbuffer	a_write_mixb;			/*!< audio mix buffer */

	int			timeout;			/*!< used to set the timeout for data */
#define DATA_READ_TIMEOUT	10000				/* 10 seconds */

	unsigned long		channel_instance;		/*!< number of channels created on this device */

	/* device caps */
	unsigned int		use_ucs2_encoding:1;

	/* device state */
	int				gsm_reg_status;
	int				act;
	int				operator;
	int				rssi;

	struct ast_format_cap* local_format_cap;

	/* SMS support */
	int	incoming_sms_index;

	/* string fields */
	AST_DECLARE_STRING_FIELDS(
		AST_STRING_FIELD(manufacturer);
		AST_STRING_FIELD(model);
		AST_STRING_FIELD(firmware);
		AST_STRING_FIELD(imei);
		AST_STRING_FIELD(imsi);
		AST_STRING_FIELD(iccid);
		AST_STRING_FIELD(network_name);
		AST_STRING_FIELD(short_network_name);
		AST_STRING_FIELD(provider_name);
		AST_STRING_FIELD(location_area_code);
		AST_STRING_FIELD(cell_id);
		AST_STRING_FIELD(band);
		AST_STRING_FIELD(sms_scenter);
		AST_STRING_FIELD(subscriber_number);
		AST_STRING_FIELD(module_time);

	);


	/* flags */
	volatile unsigned int	connected:1;			/*!< do we have an connection to a device */
	unsigned int			initialized:1;			/*!< whether a service level connection exists or not */
	unsigned int			gsm_registered:1;		/*!< do we have an registration to a GSM */
	unsigned int			dialing;				/*!< HW state; true from ATD response OK until CEND or CONN for this call idx */
	unsigned int			ring:1;					/*!< HW state; true if has incoming call from first RING until CEND or CONN */
	unsigned int			cwaiting:1;				/*!< HW state; true if has incoming call waiting from first CCWA until CEND or CONN for */
	unsigned int			outgoing_sms:1;			/*!< outgoing sms */
	unsigned int			volume_sync_step:2;		/*!< volume synchronized stage */
#define VOLUME_SYNC_BEGIN	0
#define VOLUME_SYNC_DONE	3

	unsigned int		has_sms:1;				/*!< device has SMS support */
	unsigned int		has_voice:1;			/*!< device has voice call support */
	unsigned int		is_simcom:1;			/*!< device is a simcom module */
	unsigned int		has_call_waiting:1;		/*!< call waiting enabled on device */

	unsigned int		group_last_used:1;		/*!< mark the last used device */
	unsigned int		prov_last_used:1;		/*!< mark the last used device */
	unsigned int		sim_last_used:1;		/*!< mark the last used device */

	unsigned int		terminate_monitor:1;		/*!< non-zero if we want terminate monitor thread i.e. restart, stop, remove */
	unsigned int		has_subscriber_number:1;	/*!< subscriber_number field is valid */
	unsigned int		must_remove:1;				/*!< mean must removed from list: NOT FULLY THREADSAFE */

	volatile dev_state_t	desired_state;			/*!< desired state */
	volatile restate_time_t	restart_time;			/*!< time when change state */
	volatile dev_state_t	current_state;			/*!< current state */

	pvt_config_t	settings;			/*!< all device settings from config file */
	pvt_state_t		state;				/*!< state */
	pvt_stat_t		stat;				/*!< various statistics */
} pvt_t;

#define CONF_GLOBAL(name)		(gpublic->global_settings.name)
#define SCONF_GLOBAL(state, name)	((state)->global_settings.name)

#define CONF_SHARED(pvt, name)		SCONFIG(&((pvt)->settings), name)
#define CONF_UNIQ(pvt, name)		UCONFIG(&((pvt)->settings), name)
#define PVT_ID(pvt)			UCONFIG(&((pvt)->settings), id)

#define PVT_STATE(pvt, name)		PVT_STATE_T(&(pvt)->state, name)
#define PVT_STAT(pvt, name)		PVT_STAT_T(&(pvt)->stat, name)

typedef struct public_state
{
	AST_RWLIST_HEAD(devices, pvt)	devices;
	ast_mutex_t				discovery_lock;
	pthread_t				discovery_thread;		/* The discovery thread handler */
	volatile int			unloading_flag;			/* no need mutex or other locking for protect this variable because no concurent r/w and set non-0 atomically */
	struct dc_gconfig		global_settings;
} public_state_t;

extern public_state_t * gpublic;

void clean_read_data(const char* devname, int fd, struct ringbuffer* const rb);
int pvt_get_pseudo_call_idx(const struct pvt * pvt);
int ready4voice_call(const struct pvt* pvt, const struct cpvt * current_cpvt, int opts);
int is_dial_possible(const struct pvt * pvt, int opts);

const char * pvt_str_state(const struct pvt* pvt);
struct ast_str * pvt_str_state_ex(const struct pvt* pvt);
const char * GSM_regstate2str(int gsm_reg_status);
const char * sys_act2str(int sys_submode);
const char* rssi2dBm(int rssi, char* buf, size_t len);

void pvt_on_create_1st_channel(struct pvt* pvt);
void pvt_on_remove_last_channel(struct pvt* pvt);
void pvt_reload(restate_time_t when);
int pvt_enabled(const struct pvt * pvt);
void pvt_try_restate(struct pvt * pvt);
int pvt_set_act(struct pvt* pvt, int act);

const struct ast_format* pvt_get_audio_format(const struct pvt* const);
size_t pvt_get_audio_frame_size(const struct pvt* const, int, const struct ast_format* const);
void* pvt_get_silence_buffer(struct pvt* const);

#ifdef USE_SYSV_UUCP_LOCKS
int opentty(const char* dev, char ** lockfile, int typ);
void closetty(const char* dev, int fd, char ** lockfname);
int lock_try(const char * devname, char ** lockname);
#else
int opentty (const char* dev, int typ);
void closetty(const char* dev, int fd);
#endif

struct pvt * find_device_ex(struct public_state * state, const char * name);

static inline struct pvt * find_device (const char* name)
{
	return find_device_ex(gpublic, name);
}

struct pvt * find_device_ext(const char* name);
struct pvt * find_device_by_resource_ex(struct public_state * state, const char * resource, int opts, const struct ast_channel * requestor, int * exists);

static inline struct pvt * find_device_by_resource(const char * resource, int opts, const struct ast_channel * requestor, int * exists)
{
	return find_device_by_resource_ex(gpublic, resource, opts, requestor, exists);
}

struct ast_module * self_module();

#define PVT_NO_CHANS(pvt)		(PVT_STATE(pvt, chansno) == 0)

#endif /* CHAN_QUECTEL_H_INCLUDED */
