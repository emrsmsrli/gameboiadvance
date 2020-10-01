#ifndef GAMEBOIADVANCE_FILESYSTEM_H
#define GAMEBOIADVANCE_FILESYSTEM_H

#include <filesystem>
#include <vector>

#include <gba/core/integer.h>

namespace gba {

namespace filesystem = std::filesystem;

std::vector<u8> read_file(const filesystem::path& path);
void write_file(const filesystem::path& path, const std::vector<u8>& data);

} // namespace gba

#endif //GAMEBOIADVANCE_FILESYSTEM_H
