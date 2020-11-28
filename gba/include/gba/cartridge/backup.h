#ifndef GAMEBOIADVANCE_BACKUP_H
#define GAMEBOIADVANCE_BACKUP_H

#include <gba/helper/filesystem.h>

namespace gba {

class backup {
    fs::path path_;
    vector<u8> data_;
    usize size_;

public:
    enum class type { none, detect, eeprom_4kb, eeprom_64kb, sram, flash_64kb, flash_128kb };

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
public:
    explicit backup_flash(const fs::path& pak_path, const usize size)
      : backup(pak_path, size) {}

    void write(u32 address, u8 value) noexcept final;
    [[nodiscard]] u8 read(u32 address) const noexcept final;
};

} // namespace gba

#endif //GAMEBOIADVANCE_BACKUP_H
