/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_GAMEPAK_DB_H
#define GAMEBOIADVANCE_GAMEPAK_DB_H

#include <optional>

#include <gba/cartridge/backup.h>

namespace gba::cartridge {

struct pak_db_entry {
    std::string_view game_code;
    backup::type backup_type;
    bool has_rtc;
    bool has_mirroring;
};

std::optional<pak_db_entry> query_pak_db(std::string_view game_code) noexcept;

} // namespace gba::cartridge

#endif //GAMEBOIADVANCE_GAMEPAK_DB_H
