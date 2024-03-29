/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <algorithm>

#include <test_prelude.h>
#include <gba/cartridge/backup.h>
#include <gba/core/math.h>
#include <gba/core/scheduler.h>

using namespace gba;
using namespace gba::cartridge;

namespace {

struct mock_backup : public backup {
    mock_backup(const fs::path& path, usize size) : backup(path, size) {}
    void write(u32 /*address*/, u8 /*value*/) noexcept override {}
    [[nodiscard]] u8 read(u32 /*address*/) const noexcept override { return 0_u8; }
};

struct mock_existing_rom_file {
    fs::path path;
    fs::path backup_path;

    mock_existing_rom_file(const std::string_view name, usize backup_size, u8 backup_init = 0xFF_u8)
      : path{fs::temp_directory_path() / name},
        backup_path{path.parent_path() / "backups" / name}
    {
        backup_path.replace_extension(".sav");
        fs::create_directories(backup_path.parent_path());
        const vector<u8> data{backup_size, backup_init};
        fs::write_file(backup_path, data);
    }

    ~mock_existing_rom_file() noexcept {fs::remove(backup_path); }
};

} // namespace

TEST_CASE("backup missing file")
{
    const auto path = fs::temp_directory_path() / "dummy_missing.gba";
    mock_backup b{path, 64_kb};

    CHECK(!fs::exists(path));
    CHECK(b.data().size() == 64_kb);
    CHECK(std::all_of(b.data().begin(), b.data().end(), [](u8 u) { return u == 0xFF_u8; }));
}

TEST_CASE("backup existing file")
{
    constexpr usize backup_size = 128_kb;
    mock_existing_rom_file rom_file{"dummy_existing.gba", backup_size, 0x00_u8};
    mock_backup b{rom_file.path, backup_size};

    CHECK(fs::exists(rom_file.backup_path));
    CHECK(b.data().size() == b.size());
    CHECK(std::all_of(b.data().begin(), b.data().end(), [](u8 u) { return u != 0xFF_u8; }));
}

// on purpose hardcoded literals, so we can test them too
TEST_CASE("backup_flash cmds")
{
    static constexpr u32 cmd_addr1 = 0x0E005555_u32;
    static constexpr u32 cmd_addr2 = 0x0E002AAA_u32;

    const auto cmd_start = [](backup_flash& flash) {
        flash.write(cmd_addr1, 0xAA_u8);
        flash.write(cmd_addr2, 0x55_u8);
    };

    const auto cmd = [&](backup_flash& flash, u8 cmd) {
        cmd_start(flash);
        flash.write(cmd_addr1, cmd);
    };

    mock_existing_rom_file rom_file64{"dummy_flash64.gba", 64_kb};
    mock_existing_rom_file rom_file128{"dummy_flash128.gba", 128_kb};

    backup_flash flash64{rom_file64.path, 64_kb};
    backup_flash flash128{rom_file128.path, 128_kb};

    SUBCASE("device id") {
        // ID       Name       Size
        // 0xD4BF   SST        64K
        // 0x09C2   Macronix   128K
        //
        // Identification Codes MSB=Device Type, LSB=Manufacturer.
        // dev=[E000001h], man=[E000000h] (get device & manufacturer)

        const auto check_devid = [&](backup_flash& flash, u8 manufacturer, u8 device) {
            CHECK(std::all_of(flash.data().begin(), flash.data().end(), [](u8 u) { return u == 0xFF_u8; }));

            cmd(flash, 0x90_u8);  // start devid mode
            CHECK(flash.read(0x0E000000_u32) == manufacturer);
            CHECK(flash.read(0x0E000001_u32) == device);

            cmd(flash, 0xF0_u8);  // end devid mode
            CHECK(flash.read(0x0E000000_u32) == 0xFF_u32);
            CHECK(flash.read(0x0E000001_u32) == 0xFF_u32);
        };

        check_devid(flash64, 0xBF_u8, 0xD4_u8);
        check_devid(flash128, 0xC2_u8, 0x09_u8);
    }

    SUBCASE("erase chip") {
        std::fill(flash64.data().begin(), flash64.data().end(), 0x0_u8);

        cmd(flash64, 0x80_u8);  // erase
        cmd(flash64, 0x10_u8);  // erase chip
        CHECK(std::all_of(flash64.data().begin(), flash64.data().end(), [](u8 u) { return u == 0xFF_u8; }));
    }

    SUBCASE("erase sector") {
        std::fill(flash64.data().begin(), flash64.data().end(), 0x0_u8);

        cmd(flash64, 0x80_u8);  // erase
        cmd_start(flash64);
        flash64.write(0x0E000000_u32, 0x30_u8);  // erase sector 0
        CHECK(std::all_of(flash64.data().begin(), flash64.data().begin() + 0x1000_usize, [](u8 u) { return u == 0xFF_u8; }));
        CHECK(std::all_of(flash64.data().begin() + 0x1000_usize, flash64.data().end(), [](u8 u) { return u == 0x0_u8; }));
    }

    SUBCASE("write byte") {
        cmd(flash64, 0xA0_u8);
        flash64.write(0x0E001234_u32, 0x46_u8);  // should succeed
        flash64.write(0x0E001235_u32, 0x46_u8);  // should not succeed

        CHECK(std::any_of(flash64.data().begin(), flash64.data().end(), [](u8 u) { return u != 0xFF_u8; }));
        CHECK(flash64.read(0x0E001234_u32) == 0x46_u8);
        CHECK(flash64.read(0x0E001235_u32) == 0xFF_u8);
    }

    SUBCASE("switch bank") {
        // 64
        std::fill_n(flash64.data().begin(), (64_kb).get(), 0x0_u8);
        CHECK(flash64.read(0x0E001234_u32) == 0x0_u8);

        cmd(flash64, 0xB0_u8);
        flash64.write(0x0E000000_u32, 1_u8);  // should not switch banks
        CHECK(flash64.read(0x0E001234_u32) == 0x0_u8);

        // 128
        std::fill_n(flash128.data().begin(), (64_kb).get(), 0x0_u8);
        CHECK(flash128.read(0x0E001234_u32) == 0x0_u8);

        cmd(flash128, 0xB0_u8);
        flash128.write(0x0E000000_u32, 1_u8);  // should switch banks
        CHECK(flash128.read(0x0E001234_u32) == 0xFF_u8);

        cmd(flash128, 0xB0_u8);
        flash128.write(0x0E000000_u32, 0_u8);  // should switch banks
        CHECK(flash128.read(0x0E001234_u32) == 0x0_u8);
    }
}

TEST_CASE("backup_eeprom cmds")
{
    static constexpr u64 eeprom_data = 0xFEDC'BA98'7654'3210_u64;

    // eeprom read-write addresses are unused
    const auto write_address = [](backup_eeprom& eeprom) {
        // send address (0x1), second 64-bit data boundary
        for(u32 i = 0_u8; i < 5_u32; ++i) { eeprom.write(0x0_u32, 0x0_u8); }
        eeprom.write(0x0_u32, 0x1_u8);
        CHECK(eeprom.get_addr() == 0x8_u8); // internally we store bytes so address should be multiplied by 8
    };

    mock_existing_rom_file rom_file{"dummy_eeprom.gba", 512_usize};

    backup_eeprom eeprom{rom_file.path, 512_usize};  // 8_kb will perform the same.
    scheduler dummy_scheduler;
    eeprom.set_scheduler(&dummy_scheduler);

    SUBCASE("read") {
        // manually modify the internal representation so we can verify it at the end
        memcpy(eeprom.data(), 8_usize, eeprom_data);

        CHECK(eeprom.read(0x0_u32) == 0x1_u8);  // always reads 1 when read not requested and in the settled state

        // send read request (0b11)
        eeprom.write(0x0_u32, 0x1_u8);
        eeprom.write(0x0_u32, 0x1_u8);

        write_address(eeprom);
        // end address transmission
        eeprom.write(0x0_u32, 0x0_u8);

        // first 4 bits are garbage, and should be 0
        for(u32 i = 0_u32; i < 4_u32; ++i) { CHECK(eeprom.read(0x0_u32) == 0x0_u8); }

        for(u32 i = 0_u32; i < 64_u32; ++i) {
            CHECK(((eeprom_data >> (63_u32 - i)) & 0x1_u32) == eeprom.read(0x0_u32));
        }

        // eeprom should disable read mode after 68 reads
        CHECK(eeprom.read(0x0_u32) == 0x1_u8);
    }

    SUBCASE("write") {
        // send write request (0b10)
        eeprom.write(0x0_u32, 0x1_u8);
        eeprom.write(0x0_u32, 0x0_u8);

        write_address(eeprom);

        // send data
        for(u8 bit = 0_u8; bit < 64_u8; ++bit) {
            eeprom.write(0x0_u32, narrow<u8>(bit::extract(eeprom_data, 63_u8 - bit)));
        }
        // end write transmission
        eeprom.write(0x0_u32, 0x0_u8);

        // should return 0 while settling
        CHECK(eeprom.read(0x0_u32) == 0x0_u8);
        dummy_scheduler.add_cycles(120'000_u32); // approximate value
        // should return 1 after settling
        CHECK(eeprom.read(0x0_u32) == 0x1_u8);

        CHECK(std::all_of(eeprom.data().begin(), eeprom.data().begin() + 8_usize, [](u8 data) { return data == 0xFF_u8; }));
        CHECK(memcpy<u64>(eeprom.data(), 8_usize) == eeprom_data);
    }
}
