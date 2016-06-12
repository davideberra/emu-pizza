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

#ifndef __SOUND_HDR__
#define __SOUND_HDR__

typedef struct channel_one_nr10_s
{
    uint8_t shift:3;
    uint8_t negate:1;
    uint8_t sweep_period:3;
    uint8_t spare:1;

} channel_one_nr10_t;

typedef struct channel_one_nr11_s
{
    uint8_t length_load:6;
    uint8_t duty:2;

} channel_one_nr11_t;

typedef struct channel_one_nr12_s
{
    uint8_t period:3;
    uint8_t add:1;
    uint8_t volume:4;

} channel_one_nr12_t;

typedef struct channel_one_nr13_s
{
    uint8_t frequency_lsb;

} channel_one_nr13_t;

typedef struct channel_one_nr14_s
{
    uint8_t frequency_msb:3;
    uint8_t spare:3;
    uint8_t length_enable:1;
    uint8_t trigger:1;

} channel_one_nr14_t;

typedef struct channel_two_nr21_s
{
    uint8_t length_load:6;
    uint8_t duty:2;

} channel_two_nr21_t;

typedef struct channel_two_nr22_s
{
    uint8_t period:3;
    uint8_t add:1;
    uint8_t volume:4;

} channel_two_nr22_t;

typedef struct channel_two_nr23_s
{
    uint8_t frequency_lsb;

} channel_two_nr23_t;

typedef struct channel_two_nr24_s
{
    uint8_t frequency_msb:3;
    uint8_t spare:3;
    uint8_t length_enable:1;
    uint8_t trigger:1;

} channel_two_nr24_t;


typedef struct channel_three_nr30_s
{
    uint8_t spare:7;
    uint8_t dac:1;

} channel_three_nr30_t;

typedef struct channel_three_nr31_s
{
    uint8_t length_load;

} channel_three_nr31_t;


typedef struct nr52_s
{
    
} nr52_t;

typedef struct channel_s
{
    uint8_t  active;
    uint8_t  duty;
    uint8_t  duty_idx;
    uint8_t  envelope_cnt;
    double   duty_cycles;
    double   duty_cycles_cnt;
    uint32_t length;
    uint32_t frequency;
    int16_t  sample;
    int16_t  sweep_active;
    int16_t  sweep_cnt;
    int16_t  volume;

} channel_t;

typedef struct sound_s
{
    channel_one_nr10_t *channel_one_nr10;
    channel_one_nr11_t *channel_one_nr11;
    channel_one_nr12_t *channel_one_nr12;
    channel_one_nr13_t *channel_one_nr13;
    channel_one_nr14_t *channel_one_nr14;

    channel_two_nr21_t *channel_two_nr21;
    channel_two_nr22_t *channel_two_nr22;
    channel_two_nr23_t *channel_two_nr23;
    channel_two_nr24_t *channel_two_nr24;

    channel_t           channel_one;    
    channel_t           channel_two;    

    uint32_t            fs_cycles;

} sound_t;

/* super global for audio controller */
sound_t sound;

#endif

