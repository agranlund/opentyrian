/* 
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef WITH_HW_OPL

#include <mint/osbind.h>
#include "loudness.h"

#include "file.h"
#include "lds_play.h"
#include "nortsong.h"
#include "opentyr.h"
#include "params.h"

float music_volume = 0, sample_volume = 0;

bool music_stopped = true;
unsigned int song_playing = 0;

bool audio_disabled = false;
bool music_disabled = false;
bool samples_disabled = false;

/* SYN: These shouldn't be used outside this file. Hands off! */
FILE *music_file = NULL;
Uint32 *song_offset;
Uint16 song_count = 0;


SAMPLE_TYPE *channel_buffer[SFX_CHANNELS] = { NULL };
SAMPLE_TYPE *channel_pos[SFX_CHANNELS] = { NULL };
Uint32 channel_len[SFX_CHANNELS] = { 0 };
Uint8 channel_vol[SFX_CHANNELS];

int sound_init_state = false;
int freq = 11025 * OUTPUT_QUALITY;

/* static SDL_AudioCVT audio_cvt;*/ // used for format conversion

void audio_cb( void *userdata, unsigned char *feedme, int howmuch );


static uint16_t disable_interrupts() {
    register uint16_t ret __asm__ ("d0");
    __asm__ volatile (
        "   move.w  sr,%0\n\t"
        "   or.w    #0x0700,sr\n\t"
        : "=r"(ret) : : __CLOBBER_RETURN("d0") "cc" );
    return ret;
}

static void restore_interrupts(uint16_t oldsr) {
    __asm__ volatile (
        "   move.w  sr,d0\n\t"
        "   and.w   #0xF0FF,d0\n\t"
        "   and.w   #0x0F00,%0\n\t"
        "   or.w    %0,d0\n\t"
        "   move.w  d0,sr\n\t"
        : : "d"(oldsr) : "d0", "cc" );
}

extern int lds_update(void);

static void timer_func() {
    static uint32_t elapsed = 0;
    static uint32_t last200hz = 0;
    const uint32_t rate = 14285;    // 70hz

    if (audio_disabled || music_disabled || music_stopped) {
        elapsed = 0;
        last200hz = 0;
        return;
    }

    // assume 20ms per frame (50hz)
    uint32_t microseconds = 20 * 1000UL;
    // get more accurate reading from 200hz timer if possible
    uint32_t this200hz = *((volatile uint32_t*)0x4ba);
    if (last200hz && (this200hz > last200hz)) {
        // 5ms per 200hz tick
        microseconds = (this200hz - last200hz) * 5000UL;
    }
    last200hz = this200hz;
    elapsed += microseconds;
    for (int i=0; i<8 && (elapsed >= rate); i++, elapsed -= rate) {
        lds_update();
    }
}

static void init_timer() {
    uint16_t sr = disable_interrupts();
    unsigned long* vblqueue = (unsigned long*) *((unsigned long*)0x456);
    unsigned long nvbls = *((unsigned long*)0x454);
    for (unsigned long i = 0; i < nvbls; i++) {
        if (vblqueue[i] == 0) {
            vblqueue[i] = (unsigned long) &timer_func;
            break;
        }
    }
    restore_interrupts(sr);
}

static void deinit_timer() {
    uint16_t sr = disable_interrupts();
    unsigned long* vblqueue = (unsigned long*) *((unsigned long*)0x456);
    unsigned long nvbls = *((unsigned long*)0x454);
    for (unsigned long i = 0; i < nvbls; i++) {
        if (vblqueue[i] == (unsigned long) &timer_func) {
            vblqueue[i] = 0;
            break;
        }
    }
    restore_interrupts(sr);
}

void load_song( unsigned int song_num );

bool init_audio( void )
{
	if (audio_disabled)
		return false;
	
	opl_init();
    Supexec(init_timer);
	return true;
}

void deinit_audio( void )
{
	if (audio_disabled)
		return;
	
    stop_song();
    Supexec(deinit_timer);

	for (unsigned int i = 0; i < SFX_CHANNELS; i++)
	{
		free(channel_buffer[i]);
		channel_buffer[i] = channel_pos[i] = NULL;
		channel_len[i] = 0;
	}

	lds_free();
}


void load_music( void )
{
	if (music_file == NULL)
	{
		music_file = dir_fopen_die(data_dir(), "music.mus", "rb");
		
		efread(&song_count, sizeof(song_count), 1, music_file);
		
		song_offset = malloc((song_count + 1) * sizeof(*song_offset));
		
		efread(song_offset, 4, song_count, music_file);
		song_offset[song_count] = ftell_eof(music_file);
	}
}

void load_song( unsigned int song_num )
{
	if (audio_disabled)
		return;
	
    stop_song();
	if (song_num < song_count)
	{
		unsigned int song_size = song_offset[song_num + 1] - song_offset[song_num];
		lds_load(music_file, song_offset[song_num], song_size);
	}
	else
	{
		fprintf(stderr, "warning: failed to load song %d\n", song_num + 1);
	}
}

void play_song( unsigned int song_num )
{
	if (song_num != song_playing)
	{
		load_song(song_num);
		song_playing = song_num;
	}
	
	music_stopped = false;
}

void restart_song( void )
{
	unsigned int temp = song_playing;
	song_playing = -1;
	play_song(temp);
}

void stop_song( void )
{
    if (!music_stopped) {
    	music_stopped = true;
        lds_rewind();
    }
}

void fade_song( void )
{
	/* STUB: we have no implementation of this to port */
}

void set_volume( unsigned int music, unsigned int sample )
{
	music_volume = music * (1.5f / 255.0f);
	sample_volume = sample * (1.0f / 255.0f);
}

void JE_multiSamplePlay(JE_byte *buffer, JE_word size, JE_byte chan, JE_byte vol)
{
	if (audio_disabled || samples_disabled)
		return;
	
	free(channel_buffer[chan]);
	channel_len[chan] = size * BYTES_PER_SAMPLE * SAMPLE_SCALING;
	channel_buffer[chan] = malloc(channel_len[chan]);
	channel_pos[chan] = channel_buffer[chan];
	channel_vol[chan] = vol + 1;

	for (int i = 0; i < size; i++)
	{
		for (int ex = 0; ex < SAMPLE_SCALING; ex++)
		{
#if (BYTES_PER_SAMPLE == 2)
			channel_buffer[chan][(i * SAMPLE_SCALING) + ex] = (Sint8)buffer[i] << 8;
#else  /* BYTES_PER_SAMPLE */
			channel_buffer[chan][(i * SAMPLE_SCALING) + ex] = (Sint8)buffer[i];
#endif  /* BYTES_PER_SAMPLE */
		}
	}
}

#endif /* WITH_HW_OPL */
