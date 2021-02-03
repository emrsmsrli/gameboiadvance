/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_GAMEPAK_H
#define GAMEBOIADVANCE_GAMEPAK_H

#include <memory>
#include <string_view>

#include <gba/cartridge/backup.h>
#include <gba/cartridge/rtc.h>
#include <gba/core/event/event.h>
#include <gba/helper/filesystem.h>

namespace gba::cartridge {

struct gba;

class gamepak {
    event<const fs::path&> on_load_;

    fs::path path_;
    vector<u8> pak_data_;

    rtc rtc_;

    std::unique_ptr<backup> backup_;
    backup::type backup_type_{backup::type::detect};

    std::string_view game_title_;
    std::string_view game_code_;
    std::string_view maker_code_;
    u8 main_unit_code_;
    u8 software_version_;
    u8 checksum_;

    bool loaded_ = false;
    bool has_rtc_ = false;
    bool has_mirroring_ = false;

public:
    void load(const fs::path& path);

    [[nodiscard]] event<const fs::path&>& on_load() noexcept { return on_load_; }
    [[nodiscard]] bool loaded() const noexcept { return loaded_; }
    [[nodiscard]] bool has_rtc() const noexcept { return has_rtc_; }
    [[nodiscard]] backup::type backup_type() const noexcept { return backup_type_; }

    [[nodiscard]] std::unique_ptr<backup>& backup() noexcept { return backup_; }
    [[nodiscard]] rtc& rtc() noexcept { return rtc_; }

    [[nodiscard]] vector<u8>& pak_data() noexcept { return pak_data_; }

private:
    void detect_backup_type() noexcept;
};

} // namespace gba::cartridge

#endif //GAMEBOIADVANCE_GAMEPAK_H
