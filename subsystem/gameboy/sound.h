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

#include <SDL2/SDL.h>
#include "subsystem/gameboy/globals.h"
#include "subsystem/gameboy/mmu_hdr.h"
#include "subsystem/gameboy/sound_hdr.h"

/* SDL structure */
SDL_AudioSpec desired;
SDL_AudioSpec obtained;

/* */
uint32_t sound_cycles;

uint32_t cpu_clock = 4194304;


uint32_t prot_cycles;
uint32_t samples;
uint32_t samples_req;

/* */
#define SOUND_BUF_SZ 65536
#define SOUND_FREQ   22050

int16_t  sound_buf[SOUND_BUF_SZ];
size_t   sound_buf_rd;
size_t   sound_buf_wr;

/* CPU cycles to internal cycles counters */
uint32_t sound_fs_cycles;
uint32_t sound_fs_cycles_cnt;
double   sound_sample_cycles;
double   sound_sample_cycles_cnt;

/* internal prototypes */
void sound_envelope_step();
void sound_length_ctrl_step();
void sound_read_buffer(void *userdata, uint8_t *stream, int snd_len);
void sound_push_sample(int16_t s);
void sound_read_samples(int len, int16_t *buf);
void sound_sweep_step();



int16_t miao = 10000;


/* init sound states */
void static sound_init()
{
    SDL_Init(SDL_INIT_AUDIO);
    desired.freq = SOUND_FREQ;
    desired.samples = 1024;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.callback = sound_read_buffer;
    desired.userdata = NULL;

    /* Open audio */
    if (!SDL_OpenAudio(&desired, &obtained)) {
        SDL_PauseAudio(0);
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

//    sound.nr26 = (nr26_t *) mmu_addr(0xFF26);

    sound_cycles = 0;
    prot_cycles = 0;

    /* how many cpu cycles we need to emit a 512hz clock (frame sequencer) */
    sound_fs_cycles = cpu_clock / 512;

    /* how many cpu cycles to generate a single sample? */
    sound_sample_cycles = (double) cpu_clock / SOUND_FREQ;

    printf("SAMPLE CYCLES: %f\n", sound_sample_cycles);

}

/* update sound internal state given CPU T-states */
void static __always_inline sound_step(uint8_t t)
{
    prot_cycles += t;
    sound_fs_cycles_cnt += t;
    sound_sample_cycles_cnt += (double) t;

    /* frame sequencer run at 512 hz */
    if (sound_fs_cycles_cnt >= sound_fs_cycles)
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





    /* enough cpu cycles to generate a single frame? */
    if (sound_sample_cycles_cnt >= sound_sample_cycles)
    {
        int32_t sample = 0;
        uint8_t channels = 0;

        /* time to generate a sample! sum all the fields */
        if (sound.channel_one.active)
        {
            sample += sound.channel_one.sample;
            channels++;
        }

        if (sound.channel_two.active)
        {
            sample += sound.channel_two.sample;
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

//        if (samples % 64 == 0)
//            miao *= -1;

//        sound_push_sample(miao);

        samples++;

        sound_sample_cycles_cnt -= sound_sample_cycles;
    }

    if (prot_cycles >= cpu_clock)
    {
        uint32_t available_samples;
    struct timeval tv;
    gettimeofday(&tv, NULL);

        if (sound_buf_rd > sound_buf_wr)
            available_samples = sound_buf_wr + SOUND_BUF_SZ - sound_buf_rd;
        else
            available_samples = sound_buf_wr - sound_buf_rd;

        // printf("%u.%u - ZIRO COMPLETO, CICLI %d - GENERATI %d SAMPLES - RICHIESTI %d SAMPLES - DISPONIBILI %d \n", tv.tv_sec, tv.tv_usec, prot_cycles, samples, samples_req, available_samples);

        samples = 0;
        samples_req = 0;
        prot_cycles = 0;
    }
}

/* length controller step */
void sound_length_ctrl_step()
{
    /* treat channel one */ 
    if (sound.channel_one.active)
    {
        sound.channel_one.length--;

        /* if ZERO is reached, turn off the channel */
        if (sound.channel_one.length == 0)
        {
//            printf("DISATTIVO CANALE UNO\n");
            sound.channel_one.active = 0;
        }
    }

    if (sound.channel_two.active)
    {
        sound.channel_two.length--;

        /* if ZERO is reached, turn off the channel */
        if (sound.channel_two.length == 0)
        {
//            printf("DISATTIVO CANALE DUO\n");
            sound.channel_two.active = 0;
        }
    }

}

/* generate samples after a single change on sound registers */
void sound_generate()
{
    uint32_t i;
    uint32_t samples = (sound_cycles * SOUND_FREQ) / 4194304;

    /* AT LEAST a sample? */
    if (samples == 0)
        return;

/*    uint16_t freq = sound.square_one_nr13->frequency_lsb | 
                    (sound.square_one_nr14->frequency_msb << 8); 

    printf("HZ TUONO %d - SABOTELLA %d \n", freq, ~(freq + 1));

    if (freq != 0)
    {
        uint32_t hz = 131072 / (2048 - freq);

        printf("HZ SUONO %d\n", hz);
    }

    for (i=0; i<samples; i++)
    {
        sound_push_sample((int16_t) i);
    } */
 
    /* reset cycles */
    sound_cycles = 0;
}

void sound_read_buffer(void *userdata, uint8_t *stream, int snd_len)
{
    /* test */
/*    int i;
    int16_t sample = 10000;

    for (i=0; i<snd_len; i++)
    {
        if (i % 32 == 0)
            sample *= -1;

        sound_push_sample(sample);
    } */

    samples_req += (snd_len / 2);

    sound_read_samples(snd_len / 2, (int16_t *) stream);
}

/* push a single sample data into circular buffer */
void sound_push_sample(int16_t s)
{
    sound_buf[sound_buf_wr] = s;

    sound_buf_wr = (sound_buf_wr + 1) % SOUND_BUF_SZ;
}

/* read a block of data from circular buffer */
void sound_read_samples(int len, int16_t *buf)
{
    size_t available_samples;
    int to_read = len;

    int i;
  /*  int16_t sample = 10000;

    for (i=0; i<len; i++)
    {
        if (i % 64 == 0)
            sample *= -1;
        buf[i] = sample;
    }
    
    return; */
      
    if (sound_buf_rd > sound_buf_wr)
        available_samples = sound_buf_wr + SOUND_BUF_SZ - sound_buf_rd;
    else
        available_samples = sound_buf_wr - sound_buf_rd;

    // printf("DISPONIBILI %d - WR %d - RD %d\n", available_samples, sound_buf_wr, sound_buf_rd);

    if (available_samples < len)
    {
//        printf("SAMPLES DISPONIBILI: %d - RICHIESTI %d\n", available_samples, len);
        to_read = available_samples;
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

    /*    for (i=0; i<to_read; i++)
            printf("%d ", sound_buf[sound_buf_rd + i]);
        printf("\n");  */
     
        /* a single memcpy is enough */
        memcpy(buf, &sound_buf[sound_buf_rd], to_read * 2); 

        /* update read index */
        sound_buf_rd += to_read;
    }    
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
            /* time to update frequency */
            uint32_t diff = sound.channel_one.frequency >> sound.channel_one_nr10->shift;

            /* the calculated diff must be summed or subtracted to frequency */
            if (sound.channel_one_nr10->negate)
                new_freq = sound.channel_one.frequency - diff;
            else 
                new_freq = sound.channel_one.frequency + diff;

            printf("NUOVA FRECHETE %d\n", new_freq);

            /* if freq > 2047, turn off the channel */
            if (new_freq > 2047)
                sound.channel_one.active = 0;
            else
            {
                /* update with the new frequency */
                sound.channel_one.frequency = new_freq;

                /* update the duty cycles */
                sound.channel_one.duty_cycles = (2048 - new_freq) * 4;
            }
 
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
                if (sound.channel_one.volume > 0)
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

            /* set sweep as active if period != 0 and shift != 0 */
            if (sound.channel_one_nr10->sweep_period != 0 &&
                sound.channel_one_nr10->shift != 0)
                sound.channel_one.sweep_active = 1;
            else
                sound.channel_one.sweep_active = 0;

/*
            printf("MAN DETTO DI EMETTERE UNA NOTA FREQ %d LUNGA %d DUTY PERIOD %f LENGTH ENABLE %d\n", 
                sound.channel_one.frequency,
                sound.channel_one.length,
                sound.channel_one.duty_cycles,
                sound.channel_one_nr14->length_enable);

            printf("ENVELOPE PERIOD %d ADD %d VOLUME %d\n", sound.channel_one_nr12->period,
                                                            sound.channel_one_nr12->add,
                                                            sound.channel_one_nr12->volume);

            printf("SWEEP SHIFT %d - NEGATE %d - PERIOD %d\n", sound.channel_one_nr10->shift,
                                                               sound.channel_one_nr10->negate,
                                                               sound.channel_one_nr10->sweep_period);
*/
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
/*
            printf("MAN DETTO DI EMETTERE UNA NOTA FREQ %d LUNGA %d DUTY PERIOD %f\n",
                sound.channel_two.frequency,
                sound.channel_two.length,
                sound.channel_two.duty_cycles);
*/
        }
    }

    
}

#endif 
