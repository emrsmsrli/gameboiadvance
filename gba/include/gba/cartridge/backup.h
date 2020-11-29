#ifndef GAMEBOIADVANCE_BACKUP_H
#define GAMEBOIADVANCE_BACKUP_H

#include <algorithm>

#include <gba/helper/bitflags.h>
#include <gba/helper/filesystem.h>

namespace gba {

class backup {
    fs::path path_;
    vector<u8> data_;
    usize size_;

public:
    enum class type { none, detect, eeprom_4, eeprom_64, sram, flash_64, flash_128 };

    virtual ~backup() = default;
    backup(const backup&) = default;
    backup(backup&&) = default;
    backup& operator=(const backup&) = default;
    backup& operator=(backup&&) = default;

    explicit backup(fs::path pak_path, const usize size) noexcept
      : path_{std::move(pak_path)},
        size_{size}
    {
        path_.replace_extension(".sav");
        if(fs::exists(path_)) {
            data_ = fs::read_file(path_);
        } else {
            data_.resize(size_);
        }
    }

    void write_to_file() const noexcept { fs::write_file(path_, data_); }

    [[nodiscard]] usize size() const noexcept { return size_; }
    [[nodiscard]] vector<u8>& data() noexcept { return data_; }
    [[nodiscard]] const vector<u8>& data() const noexcept { return data_; }

    virtual void write(u32 address, u8 value) noexcept = 0;
    [[nodiscard]] virtual u8 read(u32 address) const noexcept = 0;
};

class backup_eeprom : public backup {
public:
    explicit backup_eeprom(const fs::path& pak_path, const usize size)
      : backup(pak_path, size) {}

    void write(u32 address, u8 value) noexcept final;
    [[nodiscard]] u8 read(u32 address) const noexcept final;
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
    enum class cmd : u8::type { none, device_id, erase, write_byte, select_bank };

    usize current_bank_ = 0_usize;
    array<u8, 2> device_id_;
    state state_{state::accept_cmd};
    cmd current_cmds_{cmd::none};

public:
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
            LOG_ERROR("incorrect flash size");
        }
    }

    void write(u32 address, u8 value) noexcept final;
    [[nodiscard]] u8 read(u32 address) const noexcept final;

private:
    [[nodiscard]] usize physical_addr(const u32 addr) const noexcept { return current_bank_ * 64_kb + addr; }
};

ENABLE_BITFLAG_OPS(backup_flash::cmd);

constexpr std::string_view to_string_view(const backup::type type) noexcept
{
    switch(type) {
        case backup::type::none: return "none";
        case backup::type::detect: return "detect";
        case backup::type::eeprom_4: return "eeprom_4";
        case backup::type::eeprom_64: return "eeprom_64";
        case backup::type::sram: return "sram";
        case backup::type::flash_64: return "flash_64";
        case backup::type::flash_128: return "flash_128";
    }
}

} // namespace gba

#endif //GAMEBOIADVANCE_BACKUP_H
