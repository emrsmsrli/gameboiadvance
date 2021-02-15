/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_FWD_H
#define GAMEBOIADVANCE_FWD_H

namespace gba {

struct core;
class scheduler;

namespace cartridge {

class gamepak;

} // namespace cartridge

namespace arm {

class arm7tdmi;

} // namespace arm

namespace ppu {

class engine;

} // namespace ppu

namespace apu {

class engine;

} // namespace apu

} // namespace gba

#endif //GAMEBOIADVANCE_FWD_H
