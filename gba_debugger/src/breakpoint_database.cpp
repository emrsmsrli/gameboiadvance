/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba_debugger/breakpoint_database.h>

#include <algorithm>

#include <gba/arm/arm7tdmi.h>

namespace gba::debugger {

namespace {

auto find_execution_breakpoint(vector<execution_breakpoint>& execution_breakpoints, const u32 address) noexcept
{
    return std::find_if(execution_breakpoints.begin(), execution_breakpoints.end(),
      [=](const execution_breakpoint& bp) {
          return bp.address == address;
      });
}

} // namespace

bool access_breakpoint::operator==(const access_breakpoint& other) const noexcept {
    return address_range.contains(other.address_range)
      && other.access_width == access_width
      && other.access_type == access_type
      && (access_width == arm::debugger_access_width::any || other.data == data);
}

execution_breakpoint* breakpoint_database::get_execution_breakpoint(const u32 address) noexcept
{
    if(auto it = find_execution_breakpoint(execution_breakpoints_, address); it != execution_breakpoints_.end()) {
        return &*it;
    }
    return nullptr;
}

access_breakpoint* breakpoint_database::get_enabled_read_breakpoint(const u32 address,
  const arm::debugger_access_width access_width) noexcept
{
    auto it = std::find_if(access_breakpoints_.begin(), access_breakpoints_.end(),
      [&](const access_breakpoint& bp) {
          return bp.enabled
            && (bp.access_width == arm::debugger_access_width::any || bp.access_width == access_width)
            && bp.address_range.contains(address)
            && bitflags::is_set(bp.access_type, access_breakpoint::type::read);
      });
    return it != access_breakpoints_.end() ? &*it : nullptr;
}

access_breakpoint* breakpoint_database::get_enabled_write_breakpoint(const u32 address, const u32 data,
  const arm::debugger_access_width access_width) noexcept
{
    auto it = std::find_if(access_breakpoints_.begin(), access_breakpoints_.end(),
      [&](const access_breakpoint& bp) {
          return bp.enabled
            && (bp.access_width == arm::debugger_access_width::any || bp.access_width == access_width)
            && bp.address_range.contains(address)
            && bitflags::is_set(bp.access_type, access_breakpoint::type::write)
            && (!bp.data.has_value() || *bp.data == data);
      });
    return it != access_breakpoints_.end() ? &*it : nullptr;
}

void breakpoint_database::modify_execution_breakpoint(const u32 address, const bool toggle)
{
    auto it = find_execution_breakpoint(execution_breakpoints_, address);

    if(it != execution_breakpoints_.end()) {
        if(toggle) {
            it->enabled = !it->enabled;
        } else {
            execution_breakpoints_.erase(it);
        }
    } else {
        execution_breakpoints_.push_back(
          execution_breakpoint{address, 0_u32, std::nullopt, breakpoint_hit_type::log_and_suspend, true});
    }
}

bool breakpoint_database::add_execution_breakpoint(execution_breakpoint breakpoint)
{
    if(std::find(
      execution_breakpoints_.begin(),
      execution_breakpoints_.end(), breakpoint) == execution_breakpoints_.end())
    {
        execution_breakpoints_.push_back(std::move(breakpoint));
        return true;
    }
    return false;
}

void breakpoint_database::add_access_breakpoint(access_breakpoint breakpoint)
{
    const auto not_exists_with_access = [&](access_breakpoint bp,
      arm::debugger_access_width width, access_breakpoint::type type) {
        bp.access_width = width;
        bp.access_type = type;
        return std::find(
          access_breakpoints_.begin(),
          access_breakpoints_.end(), bp) == access_breakpoints_.end();
    };

    if(not_exists_with_access(breakpoint, arm::debugger_access_width::any, access_breakpoint::type::read_write)
      && not_exists_with_access(breakpoint, breakpoint.access_width, access_breakpoint::type::read_write)
      && not_exists_with_access(breakpoint, arm::debugger_access_width::any, breakpoint.access_type)
      && not_exists_with_access(breakpoint, breakpoint.access_width, breakpoint.access_type)
      ) {
        access_breakpoints_.push_back(breakpoint);
    }
}

std::string_view to_string_view(const access_breakpoint::type type) noexcept
{
    switch(type) {
        case access_breakpoint::type::read: return "read";
        case access_breakpoint::type::write: return "write";
        case access_breakpoint::type::read_write: return "read&write";
        default:
            UNREACHABLE();
    }
}

} // namespace gba::debugger
