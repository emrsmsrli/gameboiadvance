#ifndef GAMEBOIADVANCE_GAMEPAK_DB_H
#define GAMEBOIADVANCE_GAMEPAK_DB_H

#include <optional>

#include <gba/cartridge/backup.h>

namespace gba {

struct pak_db_entry {
    std::string_view game_code;
    backup::type backup_type;
    bool has_rtc;
    bool has_mirroring;
};

std::optional<pak_db_entry> query_pak_db(std::string_view game_code) noexcept;

} // namespace gba

#endif //GAMEBOIADVANCE_GAMEPAK_DB_H
