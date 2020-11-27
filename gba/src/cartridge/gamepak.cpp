#include <gba/cartridge/gamepak.h>

namespace gba {

namespace {

void copy_pak_str(std::string& out, const vector<u8>& data, const u8 start_addr, const u32 max_count) noexcept
{
    out.clear();
    for(u32 i = 0u; i < max_count; ++i) {
        const auto c = data[start_addr + i];
        if(c == 0) { break; }
        out.push_back(c.get());
    }
}

} // namespace

void gamepak::load(const fs::path& path)
{
    rom_data_ = read_file(path);
    path_ = path;
    loaded_ = true;

    copy_pak_str(game_title_, rom_data_, 0xA0_u8, 12_u32);
    copy_pak_str(game_code_, rom_data_, 0xAC_u8, 4_u32);
    copy_pak_str(maker_code_, rom_data_, 0xB0_u8, 2_u32);

    main_unit_code_ = rom_data_[0xB3_u32];
    software_version_ = rom_data_[0xBC_u32];
    checksum_ = rom_data_[0xBD_u32];

    u8 calculated_checksum;
    for(u32 addr = 0xA0_u32; addr < 0xBD_u32; ++addr) {
        calculated_checksum -= rom_data_[addr];
    }
    calculated_checksum -= 0x19_u8;

    LOG_INFO("------ gamepak ------");
    LOG_INFO("path: {}", path_.string());
    LOG_INFO("title: {}", game_title_);
    LOG_INFO("game code: AGB-{}", game_code_);
    LOG_INFO("maker code: {}", maker_code_);
    LOG_INFO("main unit code: {}", main_unit_code_);
    LOG_INFO("software version: {}", software_version_);

    if(calculated_checksum != checksum_) {
        LOG_WARN("checksum: {:02X} - incorrect, found: {:02X}", calculated_checksum, checksum_);
    } else {
        LOG_INFO("checksum: {:02X}", calculated_checksum);
    }

    LOG_INFO("---------------------");

    on_load_(path);
}

} // namespace gba
