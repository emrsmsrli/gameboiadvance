/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_BACKUP_H
#define GAMEBOIADVANCE_BACKUP_H

#include <algorithm>

#include <gba/core/fwd.h>
#include <gba/helper/filesystem.h>

namespace gba::cartridge {

class backup {
    fs::path path_;
    fs::mmap mmap_;
    usize size_;

protected:
    scheduler* scheduler_ = nullptr;

public:
    enum class type { none, detect, eeprom_undetected, eeprom_4, eeprom_64, sram, flash_64, flash_128 };

    virtual ~backup() = default;
    backup(const backup&) = delete;
    backup(backup&&) = default;
    backup& operator=(const backup&) = delete;
    backup& operator=(backup&&) = default;

    explicit backup(fs::path pak_path, const usize size) noexcept
      : path_{std::move(pak_path)},
        size_{size}
    {
        path_ = path_.parent_path() / "backups" / path_.filename();
        path_.replace_extension(".sav");

        std::error_code err;
        if(fs::exists(path_)) {
            mmap_ = fs::mmap{path_,err};
        } else {
            fs::create_directories(path_.parent_path());

            vector<u8> dummy(size_, 0xFF_u8);
            fs::write_file(path_, dummy);
            mmap_ = fs::mmap{path_,err};
        }

        if(err) {
            LOG_ERROR(backup, "could not create an mmap: {}", err.message());
            // we shouldn't continue at this point
            std::terminate();
        }
    }

    [[nodiscard]] usize size() const noexcept { return size_; }
    [[nodiscard]] fs::mmap& data() noexcept { return mmap_; }
    [[nodiscard]] const fs::mmap& data() const noexcept { return mmap_; }

    virtual void write(u32 address, u8 value) noexcept = 0;
    [[nodiscard]] virtual u8 read(u32 address) const noexcept = 0;

    virtual void set_size(usize size) noexcept;
    void set_scheduler(scheduler* s) noexcept { scheduler_ = s; }
};

class backup_eeprom : public backup {
    enum class state : u8::type {
        accepting_commands,
        transmitting_addr,
        transmitting_data,
        transmitting_ignored_bits,
        waiting_finish_bit
    };
    enum class cmd : u8::type {
        read, write, none
    };

    mutable u64 buffer_;
    u32 address_;
    u8 bus_width_;
    u8 settled_response_ = 1_u8;
    mutable u8 transmission_count_;
    mutable state state_{state::accepting_commands};
    mutable cmd cmd_{cmd::none};

public:
#if WITH_DEBUGGER
    using state_debugger = state;
    using cmd_debugger = cmd;
#endif // WITH_DEBUGGER

    // intentionally not initialize the size
    // so we can figure it out on the first write
    explicit backup_eeprom(const fs::path& pak_path)
      : backup(pak_path, 8_kb) {}
    backup_eeprom(const fs::path& pak_path, const usize size)
      : backup(pak_path, size),
        bus_width_{size == 8_kb ? 14_u8 : 6_u8} {}

    void write(u32 address, u8 value) noexcept final;
    [[nodiscard]] u8 read(u32 address) const noexcept final;

    void set_size(usize size) noexcept final;

    [[nodiscard]] u32 get_addr() const noexcept { return address_; }

private:
    void reset_buffer() const noexcept { buffer_ = 0_u64; transmission_count_ = 0_u8; }
    void on_settle(u64 /*late_cycles*/) noexcept
    {
        settled_response_ = 1_u8;

        std::error_code err;
        data().flush(err);

        if(err) {
            LOG_ERROR(eeprom, "could not flush file: {}", err.message());
        }
    }
};

class backup_sram : public backup {
public:
    explicit backup_sram(const fs::path& pak_path)
      : backup(pak_path, 32_kb) {}

    void write(u32 address, u8 value) noexcept final;
    [[nodiscard]] u8 read(u32 address) const noexcept final;
};

class backup_flash : public backup {
    enum class state : u8::type { accept_cmd, cmd_phase1, cmd_phase2, cmd_phase3 };
    enum class cmd : u8::type {
        none = 0u,
        device_id = 1u << 0u,
        erase = 1u << 1u,
        write_byte = 1u << 2u,
        select_bank = 1u << 3u
    };

    usize current_bank_ = 0_usize;
    array<u8, 2> device_id_;
    state state_{state::accept_cmd};
    cmd current_cmds_{cmd::none};

public:
#if WITH_DEBUGGER
    using state_debugger = state;
    using cmd_debugger = cmd;
    using device_id_debugger = array<u8, 2>;
#endif // WITH_DEBUGGER

    explicit backup_flash(const fs::path& pak_path, const usize size)
      : backup(pak_path, size)
    {
        // D4BFh  SST        64K
        // 09C2h  Macronix   128K
        if(size == 64_kb) {
            device_id_ = {0xBF_u8, 0xD4_u8};
        } else if(size == 128_kb) {
            device_id_ = {0xC2_u8, 0x09_u8};
        } else {
            LOG_ERROR(backup::flash, "incorrect flash size");
        }
    }

    void write(u32 address, u8 value) noexcept final;
    [[nodiscard]] u8 read(u32 address) const noexcept final;

private:
    [[nodiscard]] usize physical_addr(const u32 addr) const noexcept { return current_bank_ * 64_kb + addr; }
};

constexpr std::string_view to_string_view(const backup::type type) noexcept
{
    switch(type) {
        case backup::type::none: return "none";
        case backup::type::detect: return "detect";
        case backup::type::eeprom_undetected: return "eeprom_undetected";
        case backup::type::eeprom_4: return "eeprom_4";
        case backup::type::eeprom_64: return "eeprom_64";
        case backup::type::sram: return "sram";
        case backup::type::flash_64: return "flash_64";
        case backup::type::flash_128: return "flash_128";
        default:
            UNREACHABLE();
    }
}

} // namespace cartridge::gba

#endif //GAMEBOIADVANCE_BACKUP_H
