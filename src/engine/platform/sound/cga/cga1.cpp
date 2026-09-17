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

// cga1.cpp - implementation of CGA1-XNLP

#include "cga1.hpp"
#include <algorithm>
#include <cstdint>

namespace cga {
    void cga1_channel::set_frequency(uint32_t clocks_per_second, double frequency) {
        double cycles_per_tick = clocks_per_second / frequency;
        if (cycles_per_tick != cycles_per_tick) {
            cycles_per_tick = 0x1'00'00'00;
        }
        cycles_per_tick = std::min(std::max(cycles_per_tick, 1.0), double{0x1'00'00'00});
        this->config.timer_length = std::min((uint32_t)(cycles_per_tick - 1), uint32_t{0xffffff});
    }
    
    void cga1_channel::update_lfsr(void) {
        if (this->ticks_until_reset == 0) {
            this->ticks_until_reset = this->config.noise_period;
            this->lfsr_value = this->config.lfsr_reset_value;
        } else {
            this->ticks_until_reset--;
            uint16_t x = this->lfsr_value;
            x ^= x << 7;
            x ^= x >> 9;
            x ^= x << 8;
            this->lfsr_value = x;
        }
        this->scaled_lfsr_value = (uint16_t)((uint32_t)(this->lfsr_value) * (uint32_t)(this->config.volume) / 15);
    }
    void cga1_channel::update_lpf(void) {
        uint16_t pos = this->lpf_position;
        uint16_t target = this->scaled_lfsr_value;
        uint16_t distance = pos > target ? pos - target : target - pos;
        uint16_t speed = (uint16_t)(this->config.lpf_approach_speed) * (uint16_t)(this->config.lpf_approach_speed);
        
        if (distance < speed) {
            this->lpf_position = target;
        } else if (this->lpf_position > target) {
            this->lpf_position -= speed;
        } else {
            this->lpf_position += speed;
        }
    }
    void cga1_channel::cycle(size_t n) {
        for (size_t i = 0; i < n; i++) {
            if (this->cycles_until_tick == 0) {
                this->cycles_until_tick = this->config.timer_length;
                this->update_lfsr();
            } else {
                this->cycles_until_tick--;
            }
    
            if (this->cycles_until_lpf_approach == 0) {
                this->cycles_until_lpf_approach = this->config.lpf_approach_divider;
                this->update_lpf();
            } else {
                this->cycles_until_lpf_approach--;
            }
        }
    }

    uint16_t cga1_xnlp::mix() {
        uint16_t output = 0;
        for (size_t i = 0; i < 4; i++) {
            if (this->channels[i].config.muted) { continue; }
            output += this->channels[i].lpf_position >> 2;
        }
        return output;
    }
    void cga1_xnlp::cycle(size_t n) {
        for (size_t i = 0; i < 4; i++) {
            this->channels[i].cycle(n);
        }
    }
}
