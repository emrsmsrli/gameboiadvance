/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARM_DEBUGGER_H
#define GAMEBOIADVANCE_ARM_DEBUGGER_H

#include <optional>

#include <gba/core/fwd.h>
#include <gba/core/container.h>
#include <gba/core/event/event.h>
#include <gba/helper/range.h>
#include <gba/helper/bitflags.h>

namespace gba::debugger {

class arm_debugger {
public:
    enum class execution_request { none, instruction, scanline, frame };

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

private:
    arm::arm7tdmi* arm_;

    vector<execution_breakpoint> execution_breakpoints_;
    vector<access_breakpoint> access_breakpoints_;

public:
    event<execution_request> on_execution_requested;

    arm_debugger(arm::arm7tdmi* arm) noexcept
      : arm_{arm} {}

    void draw() noexcept;

    [[nodiscard]] execution_breakpoint* get_execution_breakpoint(u32 address) noexcept;
    [[nodiscard]] bool has_enabled_read_breakpoint(u32 address, arm::debugger_access_width access_width) noexcept;
    [[nodiscard]] bool has_enabled_write_breakpoint(u32 address, u32 data, arm::debugger_access_width access_width) noexcept;

private:
    void draw_breakpoints() noexcept;
    void draw_execution_breakpoints() noexcept;
    void draw_access_breakpoints() noexcept;
    void draw_disassembly() noexcept;
};

} // namespace gba::debugger

ENABLE_BITFLAG_OPS(gba::debugger::arm_debugger::access_breakpoint::type);

#endif //GAMEBOIADVANCE_ARM_DEBUGGER_H
