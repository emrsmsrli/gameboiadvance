/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_FILESYSTEM_H
#define GAMEBOIADVANCE_FILESYSTEM_H

#include <filesystem>

#include <gba/core/container.h>

namespace gba::fs {

using namespace std::filesystem;

vector<u8> read_file(const path& path);
void write_file(const path& path, const vector<u8>& data);

} // namespace gba::fs

#endif //GAMEBOIADVANCE_FILESYSTEM_H
