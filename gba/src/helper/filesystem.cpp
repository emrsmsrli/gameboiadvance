#include <gba/helper/filesystem.h>

#include <array>
#include <fstream>

namespace gba {

std::vector<u8> read_file(const fs::path& path)
{
    std::ifstream stream{path, std::ios::binary | std::ios::in};
    stream.unsetf(std::ios::skipws);
    if(!stream.is_open()) {
        std::terminate();
    }

    constexpr auto buffer_size = 1024u;
    std::array<u8, buffer_size> buffer;
    std::vector<u8> bytes;

    while(stream.read(reinterpret_cast<char*>(buffer.data()), buffer_size)) { // NOLINT
        std::copy_n(buffer.begin(), stream.gcount(), std::back_inserter(bytes));
    }

    return bytes;
}

void write_file(const fs::path& path, const std::vector<u8>& data)
{
    std::ofstream stream{path, std::ios::binary | std::ios::out};
    stream.unsetf(std::ios::skipws);
    if(!stream.is_open()) {
        std::terminate();
    }

    stream.write(reinterpret_cast<const char*>(data.data()), data.size()); // NOLINT
}

} // namespace gba
