/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_PPU_H
#define GAMEBOIADVANCE_PPU_H

#include <gba/core/container.h>

namespace gba {

class ppu {
    vector<u8> palette_ram{1_kb};
    vector<u8> vram{96_kb};
    vector<u8> oam{1_kb};

public:
    ppu() = default;
};

} // namespace gba

#endif //GAMEBOIADVANCE_PPU_H
