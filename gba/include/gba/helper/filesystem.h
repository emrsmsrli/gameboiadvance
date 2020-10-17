#ifndef GAMEBOIADVANCE_FILESYSTEM_H
#define GAMEBOIADVANCE_FILESYSTEM_H

#include <filesystem>

#include <gba/core/integer.h>
#include <gba/core/container.h>

namespace gba {

namespace fs = std::filesystem;

vector<u8> read_file(const fs::path& path);
void write_file(const fs::path& path, const vector<u8>& data);

} // namespace gba

#endif //GAMEBOIADVANCE_FILESYSTEM_H
