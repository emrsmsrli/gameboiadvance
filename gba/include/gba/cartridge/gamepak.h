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
#include <gba/core/fwd.h>
#include <gba/helper/filesystem.h>

namespace gba::cartridge {

class gamepak {
    friend arm::arm7tdmi;

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
    u32 mirror_mask_;

public:
    static constexpr u32 default_mirror_mask = 0x01FF'FFFF_u32;

    event<const fs::path&> on_load;

    void load(const fs::path& path);
    void write_backup() const noexcept { return backup_->write_to_file(); }

    void set_irq_controller_handle(const arm::irq_controller_handle irq) noexcept { rtc_.set_irq_controller_handle(irq); }
    void set_scheduler(scheduler* s) noexcept { backup_->set_scheduler(s); }

    [[nodiscard]] bool loaded() const noexcept { return loaded_; }
    [[nodiscard]] backup::type backup_type() const noexcept { return backup_type_; }

    void on_eeprom_bus_width_detected(backup::type eeprom_type) noexcept;

private:
    void detect_backup_type() noexcept;
};

} // namespace gba::cartridge

#endif //GAMEBOIADVANCE_GAMEPAK_H
