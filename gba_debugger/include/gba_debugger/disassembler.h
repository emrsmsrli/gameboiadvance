/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DISASSEMBLER_H
#define GAMEBOIADVANCE_DISASSEMBLER_H

#include <string>

#include <gba/core/integer.h>

namespace gba::debugger {

class disassembler {
public:
    [[nodiscard]] static std::string disassemble_arm(u32 address, u32 instruction) noexcept;
    [[nodiscard]] static std::string disassemble_thumb(u32 address, u16 instruction) noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_DISASSEMBLER_H
