/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/helper/filesystem.h>

#include <fstream>

namespace gba::fs {

vector<u8> read_file(const path& path)
{
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if(!stream.is_open()) {
        LOG_CRITICAL(fs, "input file stream could not be opened: {}", path.string());
        std::terminate();
    }

    const std::ifstream::pos_type file_size = stream.tellg();

    vector<u8> bytes{static_cast<usize::type>(file_size)};
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), file_size); // NOLINT

    LOG_TRACE(fs, "read {} bytes from {}", bytes.size(), path.string());
    return bytes;
}

void write_file(const path& path, const vector<u8>& data)
{
    std::ofstream stream{path, std::ios::binary};
    if(!stream.is_open()) {
        LOG_CRITICAL(fs, "output file stream could not be opened: {}", path.string());
        std::terminate();
    }

    stream.write(reinterpret_cast<const char*>(data.data()), data.size().get()); // NOLINT
    LOG_TRACE(fs, "wrote {} bytes to {}", data.size(), path.string());
}

} // namespace gba::fs
