/*
    pcm.h
*/

#ifndef CHAN_QUECTEL_PCM_H_INCLUDED
#define CHAN_QUECTEL_PCM_H_INCLUDED

#include <alsa/asoundlib.h>

#include <asterisk/format.h>

int pcm_init(const char* dev, snd_pcm_stream_t stream, const struct ast_format* const fmt, snd_pcm_t** pcm, unsigned int* pcm_channels, int* fd);
int pcm_close(const char* dev, snd_pcm_t** ad, snd_pcm_stream_t stream_type);

void _pcm_show_state(int attribute_unused lvl, const char* file, int line, const char* function, const char* const pcm_desc, const char* const pvt_id,
                     snd_pcm_t* const pcm);

#define pcm_show_state(level, ...)                       \
    do {                                                 \
        if (DEBUG_ATLEAST(level)) {                      \
            _pcm_show_state(AST_LOG_DEBUG, __VA_ARGS__); \
        }                                                \
    } while (0)

int pcm_status(snd_pcm_t* const pcm_playback, snd_pcm_t* const pcm_capture);

#endif
