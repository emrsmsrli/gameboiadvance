/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_MEMORY_DEBUGGER_H
#define GAMEBOIADVANCE_MEMORY_DEBUGGER_H

#include <variant>
#include <string_view>

#include <gba/core/container.h>

namespace gba::debugger {

class breakpoint_database;

struct custom_disassembly_entry {};

struct memory_view_entry {
    std::string_view name;
    vector<u8>* data;
    u32 base_addr;
};

class disassembly_view {
    breakpoint_database* bp_db_;
    vector<std::variant<custom_disassembly_entry, memory_view_entry>> entries_;

public:
    explicit disassembly_view(breakpoint_database* bp_db) noexcept
      : bp_db_(bp_db) {}

    void add_entry(const memory_view_entry& entry) noexcept { entries_.push_back(entry); }
    void add_custom_disassembly_entry() noexcept { entries_.push_back(custom_disassembly_entry{}); }
    void draw_with_mode(bool thumb_mode) noexcept;
};

class memory_view {
    vector<memory_view_entry> entries_;

public:
    void add_entry(const memory_view_entry& entry) noexcept { entries_.push_back(entry); }
    void draw() noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_MEMORY_DEBUGGER_H
