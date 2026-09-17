/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2026 tildearrow and contributors
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

#include "cga1.h"
#include "../engine.h"

void DivPlatformCGA1::configure(size_t chan) {
  DivInstrumentCGA1* ins=&this->chan[chan].insCopy;
  cga1.channels[chan].config = {
    .noise_period=ins->noisePeriod,
    .lfsr_reset_value=ins->resetValue,
    .lpf_approach_speed=this->chan[chan].active?ins->lpfApproachSpeed:(uint8_t)0,
    .lpf_approach_divider=ins->lpfApproachDivider,
    .volume=this->chan[chan].active?(uint8_t)(CLAMP(this->chan[chan].outVol,0,0xf)):(uint8_t)0,
    .muted=this->isMuted[chan],
    .use_lpf=ins->useLpf,
  };
  cga1.channels[chan].config.timer_length=this->chan[chan].freq/(cga1.channels[chan].config.noise_period+1);
}

void DivPlatformCGA1::acquire(short** buf, size_t len) {
  for (size_t i=0; i<4; i++) {
    oscBuf[i]->begin(len);
  }
  for (size_t s=0; s<len; s++) {
    for (size_t i=0; i<4; i++) {
      oscBuf[i]->putSample(s,cga1.channels[i].config.muted?0:(uint16_t)((uint32_t)(cga1.channels[i].config.use_lpf?cga1.channels[i].lpf_position:cga1.channels[i].scaled_lfsr_value)*(cga1.channels[i].config.volume)/15)+0x8000);
    }
    cga1.cycle(16);
    buf[0][s]=cga1.mix()+0x8000;
  }
  for (size_t i=0; i<4; i++) {
    oscBuf[i]->end(len);
  }
}

void DivPlatformCGA1::muteChannel(int ch, bool mute) {
  this->isMuted[ch] = mute;
  this->cga1.channels[ch].config.muted = mute;
}

int DivPlatformCGA1::dispatch(DivCommand c) {
  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_STD);
      if (c.value!=DIV_NOTE_NULL) {
        chan[c.chan].baseFreq=chan[c.chan].calcBaseFreq(c.value);
        chan[c.chan].freqChanged=true;
      }
      chan[c.chan].active=true;
      chan[c.chan].macroInit(ins);
      chan[c.chan].insCopy=ins->cga1;
      chan[c.chan].outVol=chan[c.chan].vol;
      cga1.channels[c.chan].ticks_until_reset = 0;
      cga1.channels[c.chan].cycles_until_tick = 0;
      cga1.channels[c.chan].cycles_until_lpf_approach = 0;
      break;
    }
    case DIV_CMD_NOTE_OFF:
      chan[c.chan].active=false;
      chan[c.chan].keyOff=true;
      chan[c.chan].macroInit(NULL);
      break;
    case DIV_CMD_NOTE_OFF_ENV:
    case DIV_CMD_ENV_RELEASE:
      chan[c.chan].std.release();
      break;
    case DIV_CMD_INSTRUMENT:
      if (chan[c.chan].ins!=c.value || c.value2==1) {
        chan[c.chan].ins=c.value;
        chan[c.chan].insCopy=parent->getIns(chan[c.chan].ins,DIV_INS_STD)->cga1;
        cga1.channels[c.chan].ticks_until_reset = 0;
        cga1.channels[c.chan].cycles_until_tick = 0;
        cga1.channels[c.chan].cycles_until_lpf_approach = 0;
      }
      break;
    case DIV_CMD_VOLUME:
      chan[c.chan].vol=c.value;
      if (chan[c.chan].vol>0xf) chan[c.chan].vol=0xf;
      if (!chan[c.chan].std.vol.has) {
        chan[c.chan].outVol=chan[c.chan].vol;
      }
      cga1.channels[c.chan].config.volume=chan[c.chan].vol;
      break;
    case DIV_CMD_GET_VOLUME:
      if (chan[c.chan].std.vol.has) {
        return chan[c.chan].vol;
      }
      return chan[c.chan].outVol;
      break;
    case DIV_CMD_PITCH:
      chan[c.chan].pitch=c.value;
      chan[c.chan].freqChanged=true;
      break;
    case DIV_CMD_NOTE_PORTA: {
      int destFreq=chan[c.chan].calcBaseFreq(c.value2);
      bool return2=false;
      if (destFreq>chan[c.chan].baseFreq) {
        chan[c.chan].baseFreq+=c.value;
        if (chan[c.chan].baseFreq>=destFreq) {
          chan[c.chan].baseFreq=destFreq;
          return2=true;
        }
      } else {
        chan[c.chan].baseFreq-=c.value;
        if (chan[c.chan].baseFreq<=destFreq) {
          chan[c.chan].baseFreq=destFreq;
          return2=true;
        }
      }
      chan[c.chan].freqChanged=true;
      if (return2) return 2;
      break;
    }
    case DIV_CMD_LEGATO:
      chan[c.chan].baseFreq=chan[c.chan].calcBaseFreq(c.value);
      chan[c.chan].freqChanged=true;
      break;
    case DIV_CMD_GET_VOLMAX:
      return 0xf;
      break;
    default:
      break;
  }
  return 1;
}

SharedChannel* DivPlatformCGA1::getChanState(int ch) {
  return &chan[ch];
}

DivDispatchOscBuffer* DivPlatformCGA1::getOscBuffer(int ch) {
  return oscBuf[ch];
}

void DivPlatformCGA1::notifyInsChange(int ins) {
  for (size_t i=0; i<4; i++) {
    if (chan[i].ins==ins) {
      chan[i].insChanged=true;
    }
  }
}

void DivPlatformCGA1::notifyInsDeletion(void* ins) {
  for (size_t i=0; i<4; i++) {
    chan[i].std.notifyInsDeletion((DivInstrument*)ins);
  }
}

void DivPlatformCGA1::notifyPitchTable(int sample) {
  pitchTable.init(parent->song.tuning,chipClock,1,0xffffff,true,parent->song.compatFlags.linearPitch);
}

unsigned int DivPlatformCGA1::getMaxFreq(int ch) {
  return 0xffffff;
}

void DivPlatformCGA1::setFlags(const DivConfig& flags) {
  chipClock=1000000;
  CHECK_CUSTOM_CLOCK;
  rate=chipClock/16;
  for (int i=0; i<4; i++) {
    oscBuf[i]->setRate(rate);
  }

  notifyPitchTable();
}

void DivPlatformCGA1::reset() {
  this->cga1 = cga::cga1_xnlp{};
  
  for (int i=0; i<4; i++) {
    chan[i]=DivPlatformCGA1::Channel(parent->song.compatFlags.linearPitch);
    chan[i].pitchTable=&pitchTable;
    chan[i].std.setEngine(parent);
    chan[i].vol=0xf;
  }
}

void DivPlatformCGA1::tick(bool sysTick) {
  for (size_t i=0; i<4; i++) {
    chan[i].std.next();
    if (chan[i].std.vol.had) {
      chan[i].outVol=(chan[i].vol*MIN(chan[i].std.vol.val,15))/15;
    }
    if (chan[i].std.ex1.had) {
      chan[i].insCopy.noisePeriod=CLAMP(chan[i].std.ex1.val-1,1,255);
    }
    if (chan[i].std.ex2.had) {
      chan[i].insCopy.resetValue=CLAMP(chan[i].std.ex2.val,1,65535);
    }
    if (chan[i].std.ex3.had) {
      chan[i].insCopy.lpfApproachSpeed=CLAMP(chan[i].std.ex3.val,0,255);
    }
    if (chan[i].std.ex4.had) {
      chan[i].insCopy.lpfApproachDivider=CLAMP(chan[i].std.ex4.val-1,0,255);
    }
    if (NEW_ARP_STRAT) {
      chan[i].handleArp();
    } else if (chan[i].std.arp.had && !chan[i].rawFreq) {
      if (!chan[i].inPorta) {
        chan[i].baseFreq=chan[i].calcBaseFreq(parent->calcArp(chan[i].note,chan[i].std.arp.val));
      }
      chan[i].freqChanged=true;
    }
    if (chan[i].std.pitch.had) {
      if (chan[i].std.pitch.mode) {
        chan[i].pitch2+=chan[i].std.pitch.val;
        CLAMP_VAR(chan[i].pitch2,-32768,32767);
      } else {
        chan[i].pitch2=chan[i].std.pitch.val;
      }
      chan[i].freqChanged=true;
    }
    
    if (chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff) {
      chan[i].freq=chan[i].calcFreq();
      if (!chan[i].rawFreq) {
        if (chan[i].freq>0xffffff) chan[i].freq=0xffffff;
      }
      if (chan[i].keyOn) {
        if (!chan[i].std.vol.had) {
          chan[i].outVol=chan[i].vol;
        }
        chan[i].keyOn=false;
      }
      if (chan[i].keyOff) {
        chan[i].keyOff=false;
      }
      if (chan[i].freqChanged) {
        chan[i].freqChanged=false;
      }
    }
    configure(i);
  }
}

int DivPlatformCGA1::init(DivEngine* parent, int channels, int sugRate, const DivConfig& flags) {
  this->parent=parent;
  for (int i=0; i<4; i++) {
    isMuted[i]=false;
    oscBuf[i]=new DivDispatchOscBuffer;
  }
  setFlags(flags);
  reset();
  return 4;
}

void DivPlatformCGA1::quit() {
  for (int i=0; i<4; i++) {
    delete oscBuf[i];
  }
}
