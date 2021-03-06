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

#ifndef __INPUT_HDR__
#define __INPUT_HDR__

/* prototypes */
uint8_t input_get_keys(uint8_t line);
uint8_t input_init();
void    input_set_key_left(char state);
void    input_set_key_right(char state);
void    input_set_key_up(char state);
void    input_set_key_down(char state);
void    input_set_key_a(char state);
void    input_set_key_b(char state);
void    input_set_key_select(char state);
void    input_set_key_start(char state);

#endif
