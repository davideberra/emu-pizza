/*

    This file is part of Emu-Pizza

    Emu-Pizza is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Emu-Pizza is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Emu-Pizza.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __SOUND__
#define __SOUND__

#include <errno.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <semaphore.h>
#include "subsystem/gameboy/cycles_hdr.h"
#include "subsystem/gameboy/globals.h"
#include "subsystem/gameboy/gpu_hdr.h"
#include "subsystem/gameboy/mmu_hdr.h"
#include "subsystem/gameboy/sound_hdr.h"

/* SDL structure */
SDL_AudioSpec desired;
SDL_AudioSpec obtained;

/* */
#define SOUND_SAMPLES 1024
#define SOUND_BUF_SZ  65536
#define SOUND_FREQ    48000

int16_t  sound_buf[SOUND_BUF_SZ];
size_t   sound_buf_rd;
size_t   sound_buf_wr;
int      sound_buf_available;

/* CPU cycles to internal cycles counters */
uint32_t sound_fs_cycles;
uint32_t sound_fs_cycles_cnt;
double   sound_sample_cycles;
double   sound_sample_cycles_cnt;

/* semaphore for audio sync */
pthread_cond_t    sound_cond;
pthread_mutex_t   sound_mutex;
char              sound_buffer_full = 0;
char              sound_buffer_empty = 0;

/* internal prototypes */
size_t sound_available_samples();
void   sound_envelope_step();
void   sound_length_ctrl_step();
void   sound_read_buffer(void *userdata, uint8_t *stream, int snd_len);
void   sound_push_sample(int16_t s);
void   sound_read_samples(int len, int16_t *buf);
void   sound_sweep_step();
void   sound_term();


/* init sound states */
void static sound_init()
{
    SDL_Init(SDL_INIT_AUDIO);
    desired.freq = SOUND_FREQ;
    desired.samples = SOUND_SAMPLES * 2; 
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.callback = sound_read_buffer;
    desired.userdata = NULL;

    /* Open audio */
    if (SDL_OpenAudio(&desired, &obtained) == 0)
        SDL_PauseAudio(0);
    else
    {
        printf("Cannot open audio device\n");
        quit = 1;
        return;
    }

    /* point sound structures to their memory areas */
    sound.channel_one_nr10 = (channel_one_nr10_t *) mmu_addr(0xFF10);
    sound.channel_one_nr11 = (channel_one_nr11_t *) mmu_addr(0xFF11);
    sound.channel_one_nr12 = (channel_one_nr12_t *) mmu_addr(0xFF12);
    sound.channel_one_nr13 = (channel_one_nr13_t *) mmu_addr(0xFF13);
    sound.channel_one_nr14 = (channel_one_nr14_t *) mmu_addr(0xFF14);

    sound.channel_two_nr21 = (channel_two_nr21_t *) mmu_addr(0xFF16);
    sound.channel_two_nr22 = (channel_two_nr22_t *) mmu_addr(0xFF17);
    sound.channel_two_nr23 = (channel_two_nr23_t *) mmu_addr(0xFF18);
    sound.channel_two_nr24 = (channel_two_nr24_t *) mmu_addr(0xFF19);

    sound.channel_three_nr30 = (channel_three_nr30_t *) mmu_addr(0xFF1A);
    sound.channel_three_nr31 = (channel_three_nr31_t *) mmu_addr(0xFF1B);
    sound.channel_three_nr32 = (channel_three_nr32_t *) mmu_addr(0xFF1C);
    sound.channel_three_nr33 = (channel_three_nr33_t *) mmu_addr(0xFF1D);
    sound.channel_three_nr34 = (channel_three_nr34_t *) mmu_addr(0xFF1E);

    sound.wave_table = mmu_addr(0xFF30);

    /* available samples */
    sound_buf_available = 0;

    /* init semaphore for 60hz sync */
    pthread_mutex_init(&sound_mutex, NULL);
    pthread_cond_init(&sound_cond, NULL);

    /* how many cpu cycles we need to emit a 512hz clock (frame sequencer) */
    sound_fs_cycles = cycles_clock / 512;

    /* how many cpu cycles to generate a single sample? */
    sound_sample_cycles = (double) cycles_clock / SOUND_FREQ;
}

/* update sound internal state given CPU T-states */
void static __always_inline sound_step(uint8_t t)
{
    sound_fs_cycles_cnt += t;
    sound_sample_cycles_cnt += (double) t;
 
    /* frame sequencer runs at 512 hz - 8192 ticks at standard CPU speed */
    if (sound_fs_cycles_cnt == sound_fs_cycles)
    {
        sound.fs_cycles = (sound.fs_cycles + 1) % 8;

        /* length controller works at 256hz */
        if (sound.fs_cycles % 2 == 0)
            sound_length_ctrl_step();

        /* sweep works at 128hz */
        if (sound.fs_cycles == 2 || sound.fs_cycles == 6)
            sound_sweep_step();

        /* envelope works at 64hz */
        if (sound.fs_cycles == 7)
            sound_envelope_step();
 
        sound_fs_cycles_cnt -= sound_fs_cycles;
    }

    /* update all channels */

    /* update channel one */
    if (sound.channel_one.active)
    {
        sound.channel_one.duty_cycles_cnt += (double) t;

        /* enough CPU cycles to trigger a duty step? */
        if (sound.channel_one.duty_cycles_cnt >= 
            sound.channel_one.duty_cycles)
        {
            /* recalc current samples */
            if ((sound.channel_one.duty >> sound.channel_one.duty_idx) & 0x01)
                sound.channel_one.sample = sound.channel_one.volume;
            else
                sound.channel_one.sample = -sound.channel_one.volume;

            /* step to the next duty value */
            sound.channel_one.duty_idx = (sound.channel_one.duty_idx + 1) % 8;

            /* go back */
            sound.channel_one.duty_cycles_cnt -= sound.channel_one.duty_cycles;
        }
    }

    /* update channel two */
    if (sound.channel_two.active)
    {
        sound.channel_two.duty_cycles_cnt += (double) t;

        /* enough CPU cycles to trigger a duty step? */
        if (sound.channel_two.duty_cycles_cnt >=
            sound.channel_two.duty_cycles)
        {
            /* recalc current samples */
            if ((sound.channel_two.duty >> sound.channel_two.duty_idx) & 0x01)
                sound.channel_two.sample = sound.channel_two.volume;
            else
                sound.channel_two.sample = -sound.channel_two.volume;

            /* step to the next duty value */
            sound.channel_two.duty_idx = (sound.channel_two.duty_idx + 1) % 8;

            /* go back */
            sound.channel_two.duty_cycles_cnt -= sound.channel_two.duty_cycles;
        }
    }

    /* update channel three */
    if (sound.channel_three.active)
    {
        sound.channel_three.cycles_cnt += t;

        /* enough CPU cycles to trigger a wave table step? */
        if (sound.channel_three.cycles_cnt >=
            sound.channel_three.cycles)
        {
            /* switch to the next wave sample */
            sound.channel_three.index = (sound.channel_three.index + 1) % 32;

            /* set the new current sample */
            sound.channel_three.sample = 
                sound.channel_three.wave[sound.channel_three.index];

            /* go back */
            sound.channel_three.cycles_cnt -= sound.channel_three.cycles;
        } 
    }

    /* enough cpu cycles to generate a single frame? */
    if (sound_sample_cycles_cnt >= sound_sample_cycles)
    {
        int32_t sample = 0;
        uint8_t channels = 0;

        /* time to generate a sample! sum all the fields */
        if (sound.channel_one.active && sound.channel_one.sample)
        {
            sample += sound.channel_one.sample;
            channels++;
        }

        if (sound.channel_two.active)
        {
            sample += sound.channel_two.sample;
            channels++;
        }
         
        if (sound.channel_three.active)
        {
            sample += sound.channel_three.sample;
            channels++;
        }
         
        /* at least one channel? */ 
        if (channels)
        {
            /* push the average value of all channels samples */
            sound_push_sample(sample / channels);
        }
        else
            sound_push_sample(0); 

        sound_sample_cycles_cnt -= sound_sample_cycles;
    }
}

/* length controller step */
void sound_length_ctrl_step()
{
    /* treat channel one */ 
    if (sound.channel_one.active && sound.channel_one_nr14->length_enable)
    {
        sound.channel_one.length--;

        /* if ZERO is reached, turn off the channel */
        if (sound.channel_one.length == 0)
            sound.channel_one.active = 0;
    }

    if (sound.channel_two.active && sound.channel_two_nr24->length_enable)
    {
        sound.channel_two.length--;

        /* if ZERO is reached, turn off the channel */
        if (sound.channel_two.length == 0)
            sound.channel_two.active = 0;
    }

    if (sound.channel_three.active && sound.channel_three_nr34->length_enable)
    {
        sound.channel_three.length--;

        /* if ZERO is reached, turn off the channel */
        if (sound.channel_three.length == 0)
            sound.channel_three.active = 0;
    }

}

void sound_read_buffer(void *userdata, uint8_t *stream, int snd_len)
{
    sound_read_samples(snd_len / 2, (int16_t *) stream);
}

/* push a single sample data into circular buffer */
void sound_push_sample(int16_t s)
{
    /* lock the buffer */
    pthread_mutex_lock(&sound_mutex);
   
    /* assign sample value */ 
    sound_buf[sound_buf_wr] = s;

    /* update write index */
    sound_buf_wr = (sound_buf_wr + 1) % SOUND_BUF_SZ;

    /* update available samples */
    sound_buf_available++;

    /* if it's locked and we got enough samples, unlock */
    if (sound_buffer_empty && sound_buf_available == SOUND_SAMPLES * 2)
    {
        sound_buffer_empty = 0;

        pthread_cond_signal(&sound_cond); 
    }

    /* wait for the audio to be played */
    if (sound_buf_available == 18000)
    {
        sound_buffer_full = 1;

        while (sound_buffer_full == 1)
            pthread_cond_wait(&sound_cond, &sound_mutex);
    }

    /* unlock it */
    pthread_mutex_unlock(&sound_mutex);
}

/* calculate the available samples in circula buffer */
size_t sound_available_samples()
{
    if (sound_buf_rd > sound_buf_wr)
        return sound_buf_wr + SOUND_BUF_SZ - sound_buf_rd;

    return sound_buf_wr - sound_buf_rd; 
}

/* read a block of data from circular buffer */
void sound_read_samples(int len, int16_t *buf)
{
    int to_read = len;

    if (global_benchmark)
    {
        sound_buf_rd = sound_buf_wr; 
        return;
    }

    /* lock the buffer */
    pthread_mutex_lock(&sound_mutex);
    
    /* not enough samples? read what we got */
    if (sound_buf_available < to_read)
    {
        /* stop until we got enough samples */
        sound_buffer_empty = 1;

        while (sound_buffer_empty == 1)
            pthread_cond_wait(&sound_cond, &sound_mutex);
    }

    if (sound_buf_rd + to_read > SOUND_BUF_SZ)
    {
        /* overlaps the end of the buffer? copy in 2 phases */
        size_t first_block = SOUND_BUF_SZ - sound_buf_rd;

        memcpy(buf, &sound_buf[sound_buf_rd], first_block * 2);

        memcpy(&buf[first_block], sound_buf, (to_read - first_block) * 2);

        /* set the new read index */
        sound_buf_rd = to_read - first_block;
    }
    else
    {
        /* a single memcpy is enough */
        memcpy(buf, &sound_buf[sound_buf_rd], to_read * 2); 

        /* update read index */
        sound_buf_rd += to_read;
    }    

    if (sound_buffer_full)
    {
        /* unlock write thread */
        sound_buffer_full = 0;

        /* send a signal */
        pthread_cond_signal(&sound_cond);
    }

    /* update avaiable samples */
    sound_buf_available -= to_read;

    /* unlock the buffer */
    pthread_mutex_unlock(&sound_mutex);
}

/* calc the new frequency by sweep module */
uint32_t sound_sweep_calc()
{
    uint32_t new_freq;

    /* time to update frequency */
    uint32_t diff = 
             sound.channel_one.sweep_shadow_frequency >> 
             sound.channel_one_nr10->shift;

    /* the calculated diff must be summed or subtracted to frequency */
    if (sound.channel_one_nr10->negate)
        new_freq = sound.channel_one.sweep_shadow_frequency - diff;
    else
        new_freq = sound.channel_one.sweep_shadow_frequency + diff;

    /* if freq > 2047, turn off the channel */
    if (new_freq > 2047)
        sound.channel_one.active = 0;

    /* copy new_freq into shadow register */
    sound.channel_one.sweep_shadow_frequency = new_freq;

    return new_freq;
}

/* set channel one new frequency */
void sound_set_channel_one_frequency(uint32_t new_freq)
{
    /* update with the new frequency */
    sound.channel_one.frequency = new_freq;

    /* update them also into memory */
    sound.channel_one_nr13->frequency_lsb = (uint8_t) (new_freq & 0xff);
    sound.channel_one_nr14->frequency_msb = (uint8_t) ((new_freq >> 8) & 0x07);

    /* update the duty cycles */
    sound.channel_one.duty_cycles = (2048 - new_freq) * 4;

    /* and reset them */
    sound.channel_one.duty_cycles_cnt = 0;
}

/* step of frequency sweep at 128hz */
void sound_sweep_step()
{
    uint16_t new_freq;

    if (sound.channel_one.active && sound.channel_one.sweep_active)
    {
        sound.channel_one.sweep_cnt++;

        if (sound.channel_one.sweep_cnt > sound.channel_one_nr10->sweep_period)
        {
            new_freq = sound_sweep_calc();

            /* update all the stuff related to new frequency */
            sound_set_channel_one_frequency(new_freq);

            /* update freq again (but only in shadow register) */
            sound_sweep_calc();
 
            /* reset sweep counter */
            sound.channel_one.sweep_cnt = 0;
        }
    }  
}

/* step of envelope at 64hz */
void sound_envelope_step()
{
    if (sound.channel_one.active)
    {
        /* update counter */
        sound.channel_one.envelope_cnt++;

        /* if counter reaches period, update volume */
        if (sound.channel_one.envelope_cnt == sound.channel_one_nr12->period)
        {
            if (sound.channel_one_nr12->add)
            {
                if (sound.channel_one.volume < (15 * 2048))
                    sound.channel_one.volume += 2048;
            }
            else
            {
                if (sound.channel_one.volume >= 2048)
                    sound.channel_one.volume -= 2048;
            }

            /* reset counter */
            sound.channel_one.envelope_cnt = 0;
        }
    }

    if (sound.channel_two.active)
    {
        /* update counter */
        sound.channel_two.envelope_cnt++;

        /* if counter reaches period, update volume */
        if (sound.channel_two.envelope_cnt == sound.channel_two_nr22->period)
        {
            if (sound.channel_two_nr22->add)
            {
                if (sound.channel_two.volume < (15 * 2048))
                    sound.channel_two.volume += 2048;
            }
            else
            {
                if (sound.channel_two.volume > 0)
                    sound.channel_two.volume -= 2048;
            }

            /* reset counter */
            sound.channel_two.envelope_cnt = 0;
        }
    }

}

void static __always_inline sound_write_reg(uint16_t a, uint8_t v)
{
    int i;

    switch (a)
    {
        case 0xFF11:

            sound.channel_one.length = 64 - sound.channel_one_nr11->length_load;

            break;

        case 0xFF14:
        if (v & 0x80)
        {
            uint16_t freq = sound.channel_one_nr13->frequency_lsb |
                            (sound.channel_one_nr14->frequency_msb << 8);
            uint8_t len = sound.channel_one_nr11->length_load;

            /* setting internal modules data with stuff taken from memory */
            sound.channel_one.active = 1;
            sound.channel_one.frequency = freq;

            /* qty of cpu ticks needed for a duty change (1/8 of wave cycle) */
            sound.channel_one.duty_cycles = (2048 - freq) * 4;    

            /* set the 8 phase of a duty cycle by setting 8 bits */
            switch (sound.channel_one_nr11->duty)
            {
                           /* 12.5 % */
                case 0x00: sound.channel_one.duty = 0x01;
                           break;

                           /* 25% */
                case 0x01: sound.channel_one.duty = 0x81;
                           break;

                           /* 50% */
                case 0x02: sound.channel_one.duty = 0x87;
                           break;

                           /* 75% */
                case 0x03: sound.channel_one.duty = 0x7E;
                           break;
            }

            /* start duty from the 1st bit */
            sound.channel_one.duty_idx = 0;

            /* calc length */
            sound.channel_one.length = 64 - len;

            /* base volume */
            sound.channel_one.volume = sound.channel_one_nr12->volume * 2048;

            /* reset envelope counter */
            sound.channel_one.envelope_cnt = 0;

            /* save current freq into sweep shadow register */
            sound.channel_one.sweep_shadow_frequency = freq;

            /* reset sweep timer */
            sound.channel_one.sweep_cnt = 0;

            /* set sweep as active if period != 0 or shift != 0 */
            if (sound.channel_one_nr10->sweep_period != 0 ||
                sound.channel_one_nr10->shift != 0)
                sound.channel_one.sweep_active = 1;
            else
                sound.channel_one.sweep_active = 0;

            /* if shift is != 0, calc the new frequency */
            if (sound.channel_one_nr10->shift != 0)
            {
                uint32_t new_freq = sound_sweep_calc();

                /* update all the stuff related to new frequency */
                sound_set_channel_one_frequency(new_freq);
            }
        }

        case 0xFF16:
      
            sound.channel_two.length = 64 - sound.channel_two_nr21->length_load;

            break;

        case 0xFF19:
        if (v & 0x80)
        {
            uint16_t freq = sound.channel_two_nr23->frequency_lsb |
                            (sound.channel_two_nr24->frequency_msb << 8);
            uint8_t len = sound.channel_two_nr21->length_load;

            /* setting internal modules data with stuff taken from memory */
            sound.channel_two.active = 1;
            sound.channel_two.frequency = freq;

            /* qty of cpu ticks needed for a duty change (1/8 of wave cycle) */
            sound.channel_two.duty_cycles = (2048 - freq) * 4;

            /* set the 8 phase of a duty cycle by setting 8 bits */
            switch (sound.channel_two_nr21->duty)
            {
                           /* 12.5 % */
                case 0x00: sound.channel_two.duty = 0x01;
                           break;

                           /* 25% */
                case 0x01: sound.channel_two.duty = 0x81;
                           break;

                           /* 50% */
                case 0x02: sound.channel_two.duty = 0x87;
                           break;

                           /* 75% */
                case 0x03: sound.channel_two.duty = 0x7E;
                           break;
            }

            /* start duty from the 8th bit */
            sound.channel_two.duty_idx = 0;

            /* calc length */
            sound.channel_two.length = 64 - len;

            /* base volume */
            sound.channel_two.volume = sound.channel_two_nr22->volume * 2048;

            /* reset envelope counter */
            sound.channel_two.envelope_cnt = 0;
        }

        case 0xFF1E:
        if (v & 0x80)
        {
            uint16_t freq = sound.channel_three_nr33->frequency_lsb |
                            (sound.channel_three_nr34->frequency_msb << 8);
            uint8_t len = sound.channel_three_nr31->length_load;

            /* setting internal modules data with stuff taken from memory */
            sound.channel_three.active = 1;
            sound.channel_three.frequency = freq;

            /* qty of cpu ticks needed for a wave sample change */
            sound.channel_three.cycles = (2048 - freq) * 2;

            /* init wave table index */
            sound.channel_three.index = 0;

            /* calc length */
            sound.channel_three.length = 256 - len;

            /* fill wave buffer */
            for (i=0; i<16; i++)
            {
                sound.channel_three.wave[i*2] = ((sound.wave_table[i] & 0xf0) >> 4) * (32275 / 15);
                sound.channel_three.wave[(i*2) + 1] = (sound.wave_table[i] & 0x0f) * (32275 / 15);
            }

            /* update with desired audio volume */
            for (i=0; i<32; i++)
            {
                sound.channel_three.wave[i] >>= (sound.channel_three_nr32->volume_code == 0 
                                                 ? 4 : sound.channel_three_nr32->volume_code - 1);
            }
        }
    }
}

void sound_term()
{
}

#endif 
