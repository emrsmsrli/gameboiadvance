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

struct core;
class scheduler;

namespace cartridge {

class gamepak;

} // namespace cartridge

namespace keypad {

struct keypad;

} // namespace keypad

namespace arm {

class arm7tdmi;
enum class mem_access : u32::type;

#if WITH_DEBUGGER
enum class debugger_access_width : u32::type;
#endif // WITH_DEBUGGER

} // namespace arm

namespace ppu {

class engine;

} // namespace ppu

namespace apu {

class engine;

} // namespace apu

} // namespace gba

#endif //GAMEBOIADVANCE_FWD_H
