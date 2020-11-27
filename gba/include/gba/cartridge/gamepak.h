#ifndef GAMEBOIADVANCE_GAMEPAK_H
#define GAMEBOIADVANCE_GAMEPAK_H

#include <string>

#include <gba/core/event/event.h>
#include <gba/helper/filesystem.h>

namespace gba {

struct gba;

class gamepak {
    event<const fs::path&> on_load_;

    fs::path path_;
    vector<u8> rom_data_;

    std::string game_title_;
    std::string game_code_;
    std::string maker_code_;
    u8 main_unit_code_;
    u8 software_version_;
    u8 checksum_;

    bool loaded_ = false;

public:
    void load(const fs::path& path);

    [[nodiscard]] event<const fs::path&>& on_load() noexcept { return on_load_; }
    [[nodiscard]] bool loaded() const noexcept { return loaded_; }

    [[nodiscard]] vector<u8>& rom_data() noexcept { return rom_data_; }
};

} // namespace gba

#endif //GAMEBOIADVANCE_GAMEPAK_H
