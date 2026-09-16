/**
 * Catgirl Audio - an implementation of Kali the Catgirl (Feliny)'s fantasy chips.
 * Copyright (C) 2021-2026 Kali the Catgirl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// cga1.cpp - declaration of CGA1-XNLP

#include <array>
#include <cstddef>
#include <cstdint>

namespace cga {
struct cga1_channel {
    uint32_t cycles_until_tick{}; // 24-bit, has -1 bias; a value of 1 will have a noise period of 2 ticks
    uint16_t ticks_until_reset{}; // has -1 bias
    uint16_t lfsr_value{}; // non-zero to create sound, zero to make the xorshift do nothing :P
    
    uint16_t lpf_position{}; // the position of the LPF, which attempts to approach the LFSR value. the output of the chip
    uint16_t cycles_until_lpf_approach{}; // has -1 bias
    
    struct config {
        uint32_t timer_length = 0xffff; // number of clock cycles per LFSR tick (24-bit, has -1 bias)
        uint16_t noise_period = 7; // number of LFSR ticks per reset (has -1 bias)
        uint16_t lfsr_reset_value = 0;
        
        uint16_t lpf_approach_speed = 0xffff; // how much to add to the LPF position every `lpf_approach_divider` cycles.
        uint16_t lpf_approach_divider = 0; // how many cycles before an approach iteration happens (has -1 bias)

        uint8_t volume = 0xf; // 4-bit

        bool muted = false;
    } config{};

    void update_lfsr(void); // performs a xorshift on, and writebacks to, the lfsr value
    void update_lpf(void);
    void set_frequency(uint32_t clocks_per_second, double frequency); // calculates and assigns frequency; floating-point math is host-bound
    void cycle(size_t n = 1); // performs a number of clock cycles
};
struct cga1_xnlp {
    std::array<cga1_channel, 4> channels = {
        cga1_channel{},
        cga1_channel{},
        cga1_channel{},
        cga1_channel{},
    };

    uint16_t mix();
    void cycle(size_t n = 1);
};
}
