/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_GZIP_H
#define GAMEBOIADVANCE_GZIP_H

#include <optional>

#include <gba/core/container.h>

namespace gba::gzip {

std::optional<vector<u8>> compress(const vector<u8>& uncompressed) noexcept;
std::optional<vector<u8>> uncompress(const vector<u8>& compressed) noexcept;

} // namespace gba::gzip

#endif  // GAMEBOIADVANCE_GZIP_H
