/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <string_view>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <access_private.h>

#include <gba/core.h>

using regs_t = gba::array<gba::u32, 16>;
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, regs_t, r_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, wram_)

TEST_CASE("test roms")
{
     for(const auto& file : gba::fs::directory_iterator{gba::fs::current_path() / "res"}) {
         const auto& path = file.path();
         if(auto ext = path.extension(); ext == ".gba") {
             MESSAGE("testing: ", path.string());

             gba::core g{{}};
             g.load_pak(path);

             REQUIRE(g.pak.loaded());

             using namespace gba::integer_literals;

             auto& wram = access_private::wram_(g.arm);
             gba::u32& r12 = access_private::r_(g.arm)[12_u32];

             while(true) {
                g.tick_one_frame();

                if(r12 == 0_u32) {
                    break;
                }

                using namespace gba::integer_literals;
                if(wram[0_usize] != 0_u8) {
                    // wait a little longer
                    g.tick_one_frame();
                    g.tick_one_frame();
                    INFO("failed test no: ", r12.get());

                    std::string log{reinterpret_cast<const char*>(wram.data()), 4}; // NOLINT
                    log += ' ';
                    log += std::string{reinterpret_cast<const char*>(wram.data() + 4), 8}; // NOLINT
                    log += fmt::format("\ninitial r0 {:08X}\n", gba::memcpy<gba::u32>(wram, 16_usize));
                    log += fmt::format("initial r1 {:08X}\n", gba::memcpy<gba::u32>(wram, 20_usize));
                    log += fmt::format("initial r2 {:08X}\n", gba::memcpy<gba::u32>(wram, 24_usize));
                    log += fmt::format("initial cpsr {:08X}\n", gba::memcpy<gba::u32>(wram, 28_usize));
                    log += fmt::format("got/expected r3 {:08X}|{:08X}\n",
                      gba::memcpy<gba::u32>(wram, 32_usize), gba::memcpy<gba::u32>(wram, 48_usize));
                    log += fmt::format("got/expected r4 {:08X}|{:08X}\n",
                      gba::memcpy<gba::u32>(wram, 36_usize), gba::memcpy<gba::u32>(wram, 52_usize));
                    log += fmt::format("got/expected cpsr {:08X}|{:08X}\n",
                      gba::memcpy<gba::u32>(wram, 44_usize), gba::memcpy<gba::u32>(wram, 60_usize));
                    FAIL(log);
                }
             }
         }
     }
}
