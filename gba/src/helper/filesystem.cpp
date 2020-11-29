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
    std::ifstream stream{path, std::ios::binary | std::ios::in};
    stream.unsetf(std::ios::skipws);
    if(!stream.is_open()) {
        LOG_CRITICAL("input file stream could not be opened: {}", path.string());
        std::terminate();
    }

    constexpr auto buffer_size = 1024u;
    array<u8, buffer_size> buffer;
    vector<u8> bytes;

    while(stream.read(reinterpret_cast<char*>(buffer.data()), buffer_size)) { // NOLINT
        std::copy_n(buffer.begin(), stream.gcount(), std::back_inserter(bytes.underlying_data()));
    }

    LOG_TRACE("read {} bytes from {}", bytes.size(), path.string());
    return bytes;
}

void write_file(const path& path, const vector<u8>& data)
{
    std::ofstream stream{path, std::ios::binary | std::ios::out};
    stream.unsetf(std::ios::skipws);
    if(!stream.is_open()) {
        LOG_CRITICAL("output file stream could not be opened: {}", path.string());
        std::terminate();
    }

    stream.write(reinterpret_cast<const char*>(data.data()), data.size().get()); // NOLINT
    LOG_TRACE("wrote {} bytes to {}", data.size(), path.string());
}

} // namespace gba::fs
