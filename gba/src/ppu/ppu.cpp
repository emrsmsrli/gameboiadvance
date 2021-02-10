/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/ppu/ppu.h>

#include <gba/core/scheduler.h>

namespace gba::ppu {

namespace {

/*
The drawing time for each dot is 4 CPU cycles.

  Visible     240 dots,  57.221 us,    960 cycles - 78% of h-time
  H-Blanking   68 dots,  16.212 us,    272 cycles - 22% of h-time
  Total       308 dots,  73.433 us,   1232 cycles - ca. 13.620 kHz
*/
constexpr u64 cycles_hdraw = 960_u64;
constexpr u64 cycles_hblank = 272_u64;
constexpr u64 total_htime = cycles_hdraw + cycles_hblank;

} // namespace

engine::engine(scheduler* scheduler) noexcept
  : scheduler_{scheduler}
{
    scheduler_->add_event(cycles_hdraw, {connect_arg<&engine::on_hblank>, this});
}

void engine::on_hdraw(const u64 cycles_late)
{
    scheduler_->add_event(cycles_hdraw - cycles_late, {connect_arg<&engine::on_hblank>, this});
}

void engine::on_hblank(const u64 cycles_late)
{
    scheduler_->add_event(cycles_hblank - cycles_late, {connect_arg<&engine::on_hdraw>, this});
}

} // namespace gba::ppu
