/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_FWD_H
#define GAMEBOIADVANCE_FWD_H

#include <gba/core/integer.h>

namespace gba {

class core;
class scheduler;

namespace cartridge {

class gamepak;

} // namespace cartridge

namespace keypad {

struct keypad;

} // namespace keypad

namespace cpu {

static constexpr u32 clock_speed = 1_u32 << 24_u32; // 16.78 MHz

struct bus_interface;
class arm7tdmi;
class cpu;
enum class mem_access : u32::type;

#if WITH_DEBUGGER
enum class debugger_access_width : u32::type;
#endif // WITH_DEBUGGER

} // namespace cpu

namespace timer {

class controller;
class timer;

} // namespace timer

namespace dma {

class controller;
enum class occasion : u32::type;

} // namespace timer

namespace ppu {

class engine;

} // namespace ppu

namespace apu {

class engine;

} // namespace apu

} // namespace gba

#endif //GAMEBOIADVANCE_FWD_H
