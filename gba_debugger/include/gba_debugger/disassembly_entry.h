/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DISASSEMBLY_ENTRY_H
#define GAMEBOIADVANCE_DISASSEMBLY_ENTRY_H

#include <gba/core/integer.h>

namespace gba::debugger {

class breakpoint_database;

struct disassembly_entry {
    breakpoint_database* bp_db;
    u32 addr;
    u32 instr;
    bool is_thumb;
    bool is_pc = false;

    void draw() noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_DISASSEMBLY_ENTRY_H
