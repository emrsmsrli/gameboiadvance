/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_BREAKPOINT_DATABASE_H
#define GAMEBOIADVANCE_BREAKPOINT_DATABASE_H

#include <optional>

#include <gba/core/container.h>
#include <gba/core/fwd.h>
#include <gba/helper/range.h>
#include <gba/helper/bitflags.h>

namespace gba::debugger {

struct execution_breakpoint {
    u32 address;
    u32 hit_count;
    std::optional<u32> hit_count_target;
    bool enabled = true;

    bool operator==(const execution_breakpoint& other) const noexcept { return other.address == address; }
};

struct access_breakpoint {
    enum class type { read = 1, write = 2, read_write = read | write };

    range<u32> address_range{1u};
    arm::debugger_access_width access_width;
    type access_type{type::read};
    std::optional<u32> data;
    bool enabled = true;

    bool operator==(const access_breakpoint& other) const noexcept;
};

class breakpoint_database {
    vector<execution_breakpoint> execution_breakpoints_;
    vector<access_breakpoint> access_breakpoints_;

public:
    [[nodiscard]] vector<execution_breakpoint>& get_execution_breakpoints() noexcept { return execution_breakpoints_; }
    [[nodiscard]] vector<access_breakpoint>& get_access_breakpoints() noexcept { return access_breakpoints_; }

    [[nodiscard]] execution_breakpoint* get_execution_breakpoint(u32 address) noexcept;
    [[nodiscard]] bool has_enabled_read_breakpoint(u32 address, arm::debugger_access_width access_width) noexcept;
    [[nodiscard]] bool has_enabled_write_breakpoint(u32 address, u32 data, arm::debugger_access_width access_width) noexcept;

    void modify_execution_breakpoint(u32 address, bool toggle);
    bool add_execution_breakpoint(execution_breakpoint breakpoint);
    void add_access_breakpoint(access_breakpoint breakpoint);
};

std::string_view to_string_view(const access_breakpoint::type type) noexcept;

} // namespace gba::debugger

ENABLE_BITFLAG_OPS(gba::debugger::access_breakpoint::type);

#endif //GAMEBOIADVANCE_BREAKPOINT_DATABASE_H
