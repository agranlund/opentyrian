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

#if defined(TARGET_ATARI) && defined(WITH_SOUNDBLASTER)

#include <mint/osbind.h>
#include "loudness.h"
#include "file.h"
#include "lds_play.h"
#include "nortsong.h"
#include "opentyr.h"
#include "params.h"

#define ISA_EXCLUDE_LIB_MSDOS
#include "isa.h"
#include "opl.h"

uint16_t music_volume_fp = 0;
uint16_t sample_volume_fp = 0;

bool music_stopped = true;
unsigned int song_playing = 0;

bool audio_disabled = false;
bool music_disabled = false;
bool samples_disabled = false;

static FILE *music_file = NULL;
static uint32_t *song_offset;
static uint16_t song_count = 0;

#define TYRIAN_FREQ 11025
#define FREQ_DBL    1

typedef struct
{
    uint8_t* buf;
    uint8_t* pos;
    uint32_t len;
    uint8_t  vol;
} sfxchannel_t;

sfxchannel_t sfxchannels[SFX_CHANNELS];


/* ----------------------------------------------------------------------------
 * 
 * Helpers
 * 
 * --------------------------------------------------------------------------*/

static uint16_t disable_interrupts()
{
    uint16_t ret;
    __asm__ volatile (
        "   move.w  sr,%0\n\t"
        "   or.w    #0x0700,sr\n\t"
        : "=d"(ret) : : "cc" );
    return ret;
}

static void restore_interrupts(uint16_t oldsr)
{
#if 1
    __asm__ volatile (
        "   move.w  %0,sr\n\t"
        : : "d"(oldsr) : "d0", "cc" );
#else
    __asm__ volatile (
        "   move.w  sr,d0\n\t"
        "   and.w   #0xF0FF,d0\n\t"
        "   and.w   #0x0F00,%0\n\t"
        "   or.w    %0,d0\n\t"
        "   move.w  d0,sr\n\t"
        : : "d"(oldsr) : "d0", "cc" );
#endif        
}

/* ----------------------------------------------------------------------------
 * 
 * Soundblaster hardware
 * 
 * --------------------------------------------------------------------------*/
static uint32_t isa_iobase = 0;
static inline void outp(uint16_t port, uint8_t data) { *((volatile uint8_t*)(isa_iobase + port)) = data; }
static inline uint8_t inp(uint16_t port) { return *((volatile uint8_t*)(isa_iobase + port)); } 


/* ----------------------------------------------------------------------------
 * 
 * Soundblaster hardware
 * 
 * --------------------------------------------------------------------------*/

static uint16_t blaster_port = 0;
static inline void blaster_write(uint16_t reg, uint8_t data) { outp(blaster_port + reg, data); }
static inline uint8_t blaster_read(uint16_t reg) { return inp(blaster_port + reg); }
static inline void blaster_write_dsp(uint8_t data) {
    while ((blaster_read(0xC) & 0x80) != 0) { }
    blaster_write(0xC, data);
}

static uint16_t blaster_detect()
{
    blaster_port = 0;
    for (uint16_t i = 0x220; (i<=0x280) && (blaster_port == 0); i += 0x10) {
        uint16_t temp = inp(i + 0x6);
        outp(i + 0x6, 0x1); isa_delay(3000);
        outp(i + 0x6, 0x0); isa_delay(3000);
        if (inp(i + 0xA) == 0xAA) {
            blaster_port = i;
        } else {
            outp(i + 0x6, temp); isa_delay(3000);
        }
    }
    return blaster_port;
}

static void blaster_init( void )
{
    if (blaster_detect() == 0)
    {
        printf("No Soundblaster detected\n");
        return;
    }

    printf("Soundblaster detected on port %03x\n", blaster_port);
    blaster_write_dsp(0xD1);
    blaster_write_dsp(0xF0);
}

/* ----------------------------------------------------------------------------
 * 
 * Adlib hardware
 * 
 * --------------------------------------------------------------------------*/

static uint16_t opl_port = 0x388;
static int opl_delay_idx = 8;
static int opl_delay_val = 32;
static int opl_chip = 0;

static inline void adlib_delay( unsigned long usec )
{
    isa_delay(usec);
}

int adlib_detect( unsigned short port )
{
    opl_chip = 2;
    opl_port = port;
    adlib_write(0x04, 0x60);    // reset timer1+2 and irq
    adlib_write(0x04, 0x80);
    unsigned char result1 = inp(opl_port);
    adlib_write(0x02, 0xff);    // set timer1
    adlib_write(0x04, 0x21);    // start timer1
    adlib_delay(2000);
    unsigned char result2 = inp(opl_port);
    adlib_write(0x04, 0x60);    // reset timer1+2 and irq
    adlib_write(0x04, 0x80);

    opl_chip = 0;
    if (((result1 & 0xe0) == 0x00) && ((result2 & 0xe0) == 0xc0))
    {
        opl_chip = 2;
        if ((result2 & 0x06) == 0x00)
        {
            opl_chip = 3;
        }
    }
    return opl_chip;
}

void adlib_init(Bit32u samplerate)
{
    (void)samplerate;
    static bool inited = false;
    if (inited)
        return;
    inited = true;

    if (adlib_detect(0x388) == 0)
    {
        printf("No Adlib detected\n");
        return;
    }

    printf("Adlib (OPL%d) detected on port %03x\n", opl_chip, opl_port);
    if (opl_chip == 3)
    {
        opl_delay_idx = 8;
        opl_delay_idx = 8;
    }
}

void adlib_write(Bitu idx, Bit8u val)
{
    if (opl_chip != 0)
    {
        if (idx >= 0x100) {
            // opl3 second register set
            outp(opl_port + 2, (idx - 0x100));
            adlib_delay(opl_delay_idx);
            outp(opl_port + 3, val);
            adlib_delay(opl_delay_val);
        } else {
            // opl2 / opl3 first register setc
            outp(opl_port + 0, idx);
            adlib_delay(opl_delay_idx);
            outp(opl_port + 1, val);
            adlib_delay(opl_delay_val);
        }
    }
}

Bitu adlib_reg_read(Bitu port) { /* unused */ (void) port; return 0; }
void adlib_write_index(Bitu port, Bit8u val) { /* unused */ (void)port; (void)val; }


/* ----------------------------------------------------------------------------
 * 
 * Music
 * 
 * --------------------------------------------------------------------------*/

extern int lds_update(void);

static void music_update_vbl( void )
{
    static uint32_t elapsed = 0;
    static uint32_t last200hz = 0;
    const uint32_t rate = 14285;                            // Tyrian does 70hz music updates

    if (audio_disabled || music_disabled || music_stopped)
    {
        elapsed = 0;
        last200hz = 0;
        return;
    }

    uint32_t microseconds = 20 * 1000UL;                    // assume 20ms per frame (50hz)
    uint32_t this200hz = *((volatile uint32_t*)0x4ba);      // get more accurate reading from 200hz timer if possible
    if (last200hz && (this200hz > last200hz))
    {
        microseconds = (this200hz - last200hz) * 5000UL;    // 5ms per 200hz tick
    }

    last200hz = this200hz;
    elapsed += microseconds;

    if (!music_disabled && !music_stopped)
    {
        for (int i=0; i<8 && (elapsed >= rate); i++, elapsed -= rate)
        {
            lds_update();
        }
    }    
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
    if (!music_stopped)
    {
    	music_stopped = true;
        lds_rewind();
    }
}

void fade_song( void )
{
	/* STUB: we have no implementation of this to port */
}


/* ----------------------------------------------------------------------------
 * 
 * Sound effects
 * 
 * --------------------------------------------------------------------------*/

#define _soundChunkSize     512

typedef struct
{
    volatile uint8_t*   play;               /* playback pointer */
    volatile uint8_t*   write;              /* write pointer */
    volatile uint8_t*   dsp;                /* dsp write register pointer */
    uint8_t*            mem;                /* memory buffer */
} blaster_t;

blaster_t                   blaster;
static uint32_t             _timerA_oldvec;
static uint32_t             _timerA_freq;
static uint8_t              _timerA_ctrl;
static uint8_t              _timerA_data;

static void __attribute__ ((interrupt)) timerA_blaster( void )
{
    if (blaster.play != blaster.write) {
        *blaster.dsp = 0x10;
        while(*blaster.dsp & 0x80);
        *blaster.dsp = *blaster.play;
        ((uint16_t*)&blaster.play)[1]++;
    }
    __asm__ volatile (" move.b #0xdf,0xfa0f.w\n" : : : "cc" );
}

uint16_t timerA_calc(uint16_t hz, uint8_t* ctrl, uint8_t* data)
{
    uint16_t best_hz = 0; uint8_t best_ctrl = 1; uint8_t best_data = 1;
	const uint32_t mfp_dividers[7] = {4, 10, 16, 50, 64, 100, 200};
	const uint32_t mfp_clock = 2457600;
    int best_diff = 0xFFFFFF;
    for (int i=7; i!=0; i--)
	{
		uint32_t val0 = mfp_clock / mfp_dividers[i-1];
		for (int j=1; j<256; j++)
		{
			int val = val0 / j;
			int diff = (hz > val) ? (hz - val) : (val - hz);
			if (diff < best_diff)
			{
                best_hz = val;
				best_diff = diff;
				best_ctrl = i;
				best_data = j;
			}
			if (val < hz)
				break;
		}
	}
    if (ctrl) { *ctrl = best_ctrl; }
    if (data) { *data = best_data; }
    return best_hz;
}

void timerA_enable( bool enable )
{
    uint16_t ipl = disable_interrupts();
    *((volatile uint8_t*)0x00fffa19) = enable ? _timerA_ctrl : 0;
    restore_interrupts(ipl);
}

static void sound_deinit( void )
{
    if (blaster_port == 0)
        return;

    uint16_t ipl = disable_interrupts();
    *((volatile uint8_t*)0x00fffa19) = 0;
    uint32_t base = *((volatile uint8_t*)0x00fffa17) & 0xF0;
    uint32_t vecp = ((base + 13) * 4);
    *((volatile uint32_t*)vecp) = _timerA_oldvec;

	for (unsigned int i = 0; i < SFX_CHANNELS; i++)
	{
        sfxchannels[i].buf = sfxchannels[i].pos = NULL;
        sfxchannels[i].len = 0;
	}

    if (blaster.mem) {
        free(blaster.mem);
        blaster.mem = NULL;
        blaster.play = NULL;
        blaster.write = NULL;
    }

    restore_interrupts(ipl);
}

static void sound_init( void )
{
    blaster_init();
    if (blaster_port != 0)
    {
        blaster.mem = malloc(2*64*1024);
        blaster.play = (volatile uint8_t*) ((((uint32_t)blaster.mem) + 0xffffUL) & 0xffff0000UL);
        blaster.write = blaster.play;
        blaster.dsp = (volatile uint8_t*) (isa_if->iobase + blaster_port + 0xC);

        uint16_t ipl = disable_interrupts();
        _timerA_freq = timerA_calc(TYRIAN_FREQ, &_timerA_ctrl, &_timerA_data);
        *((volatile uint8_t*)0x00fffa19) = 0;
        *((volatile uint8_t*)0x00fffa07) |= (1 << 5);
        *((volatile uint8_t*)0x00fffa13) |= (1 << 5);
        *((volatile uint8_t*)0x00fffa1f) = _timerA_data;

        uint32_t base = *((volatile uint8_t*)0x00fffa17) & 0xF0;
        uint32_t vecp = ((base + 13) * 4);
        _timerA_oldvec = *((volatile uint32_t*)vecp);
        *((volatile uint32_t*)vecp) = (uint32_t)timerA_blaster;
        restore_interrupts(ipl);
        timerA_enable(true);
    }
}


static void sound_update_vbl()
{
    if (audio_disabled || samples_disabled)
        return;

    uint32_t start = (uint32_t)blaster.write;
    uint32_t end   = (uint32_t)blaster.play;
    uint32_t bfree = ((end > start) ? (end - start) : ((end + 0x10000) - start)) & ~3;
    uint32_t bused = (0x10000 - bfree);
    uint32_t howmuch = (bused < _soundChunkSize) ? (_soundChunkSize - bused) : 0;

    if (blaster.play == blaster.write) {
        timerA_enable(false);
    }

    if (howmuch >= 16)
    {
        //howmuch -= 1;
        volatile uint32_t* clearptr = (volatile uint32_t*)blaster.write;
        for (uint32_t i=0; i<howmuch; i++) {
            *clearptr = 0x00;
             ((uint16_t*)&clearptr)[1] += 1;
        }

        uint32_t written = 0;
        for (int ch = 0; ch < SFX_CHANNELS; ch++)
        {
            sfxchannel_t* sfxchannel = &sfxchannels[ch];
            uint16_t volume = sample_volume_fp * sfxchannel->vol;
            uint32_t qu = howmuch > sfxchannel->len ? sfxchannel->len : howmuch;
            if (qu > written) {
                written = qu;
            }

            volatile uint8_t* writeptr = blaster.write;
            volatile uint8_t* readptr = sfxchannel->pos;
            for (uint32_t smp = 0; smp < qu; smp += 1) {
                uint16_t sample = (*readptr++ ^ 0x80);
                uint32_t clip = *writeptr + ((sample * volume) >> 12);
                if (clip > 0xff) {
                    clip = 0xff;
                }
                *writeptr = (uint8_t)clip;
                ((uint16_t*)&writeptr)[1] += 1;
            }
            sfxchannel->pos += qu;
            sfxchannel->len -= qu;
            if (sfxchannel->len <= 0) {
                sfxchannel->len = 0;
                sfxchannel->buf = sfxchannel->pos = NULL;
            }
        }

        if (written) {
            ((uint16_t*)&blaster.write)[1] += written;
            timerA_enable(true);
        }
    }
}


void JE_multiSamplePlay(JE_byte *buffer, JE_word size, JE_byte chan, JE_byte vol)
{
	if (audio_disabled || samples_disabled)
		return;
	
    sfxchannel_t* channel = &sfxchannels[chan];
    channel->len = size & ~1;
    channel->buf = buffer;
    channel->pos = buffer;
    channel->vol = vol + 1;
}



/* ----------------------------------------------------------------------------
 * 
 * Loudness
 * 
 * --------------------------------------------------------------------------*/

static void timer_func_vbl( void )
{
    music_update_vbl();
    sound_update_vbl();
}

static void init_timer_vbl( void ) {
    uint16_t sr = disable_interrupts();
    unsigned long* vblqueue = (unsigned long*) *((unsigned long*)0x456);
    unsigned long nvbls = *((unsigned long*)0x454);
    for (unsigned long i = 0; i < nvbls; i++)
    {
        if (vblqueue[i] == 0)
        {
            vblqueue[i] = (unsigned long) &timer_func_vbl;
            break;
        }
    }
    restore_interrupts(sr);
}

static void deinit_timer_vbl( void )
{
    uint16_t sr = disable_interrupts();
    unsigned long* vblqueue = (unsigned long*) *((unsigned long*)0x456);
    unsigned long nvbls = *((unsigned long*)0x454);
    for (unsigned long i = 0; i < nvbls; i++)
    {
        if (vblqueue[i] == (unsigned long) &timer_func_vbl)
        {
            vblqueue[i] = 0;
            break;
        }
    }
    restore_interrupts(sr);
}

long init_audio_super( void )
{
	if (audio_disabled)
		return 1;
	
    if (!isa_init() || (isa_if->iobase == 0))
    {
        audio_disabled = true;
        return 1;
    }

    isa_iobase = isa_if->iobase;

	opl_init();
    sound_init();
    init_timer_vbl();
	return 0;
}

void deinit_audio_super( void )
{
	if (audio_disabled)
		return;
	
    stop_song();
    deinit_timer_vbl();
    sound_deinit();
	lds_free();
}

bool init_audio( void )
{
    printf("init audio\n");
    return (Supexec(init_audio_super) == 0) ? false : true;
}

void deinit_audio ( void )
{
    Supexec(deinit_audio_super);
}



extern uint16_t lds_volume;
void set_volume( unsigned int music, unsigned int sample )
{
    //music_volume_fp = music > 255 ? 255 : music;
    //lds_volume = (music > 255) ? 255 : music;
	sample_volume_fp = sample > 255 ? 255 : sample;
}

#endif /* TARGET_ATARI && WITH_SOUNDBLASTER */
