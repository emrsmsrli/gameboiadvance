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

ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::u32, r15_)
ACCESS_PRIVATE_FIELD(gba::arm::arm7tdmi, gba::vector<gba::u8>, wram_)

TEST_CASE("test roms")
{
     for(const auto& file : gba::fs::directory_iterator{gba::fs::current_path() / "res"}) {
         const auto& path = file.path();
         if(auto ext = path.extension(); ext == ".gba") {
             gba::core g{{}};
             g.load_pak(path);

             REQUIRE(g.pak.loaded());

             auto& wram = access_private::wram_(g.arm);
             gba::u32& pc = access_private::r15_(g.arm);
             gba::u32 last_pc;

             while(true) {
                g.tick();

                if(last_pc == pc) {
                    break;
                }

                using namespace gba::integer_literals;
                if(wram[0_usize] != 0_u8) {
                    // wait a little longer
                    for(gba::u32 i = 0_u32; i < 200'000_u32; ++i) {
                        g.tick();
                    }

                    std::string log{reinterpret_cast<const char*>(wram.data()), 4}; // NOLINT
                    log += ' ';
                    log += std::string{reinterpret_cast<const char*>(wram.data() + 4), 8} + '\n'; // NOLINT
                    log += fmt::format("initial r0 {:08X}", gba::memcpy<gba::u32>(wram, 16_usize)) + '\n';
                    log += fmt::format("initial r1 {:08X}", gba::memcpy<gba::u32>(wram, 20_usize)) + '\n';
                    log += fmt::format("initial r2 {:08X}", gba::memcpy<gba::u32>(wram, 24_usize)) + '\n';
                    log += fmt::format("initial cpsr {:08X}", gba::memcpy<gba::u32>(wram, 28_usize)) + '\n';
                    log += fmt::format("gotten r3 {:08X}", gba::memcpy<gba::u32>(wram, 32_usize)) + '\n';
                    log += fmt::format("gotten r4 {:08X}", gba::memcpy<gba::u32>(wram, 36_usize)) + '\n';
                    log += fmt::format("gotten cpsr {:08X}", gba::memcpy<gba::u32>(wram, 44_usize)) + '\n';
                    log += fmt::format("expected r3 {:08X}", gba::memcpy<gba::u32>(wram, 48_usize)) + '\n';
                    log += fmt::format("expected r4 {:08X}", gba::memcpy<gba::u32>(wram, 52_usize)) + '\n';
                    log += fmt::format("expected cpsr {:08X}", gba::memcpy<gba::u32>(wram, 60_usize)) + '\n';
                    FAIL(log);
                }

                last_pc = pc;
             }
         }
     }
}
