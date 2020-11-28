#include <gba/cartridge/gamepak.h>

namespace gba {

namespace {

std::string_view make_pak_str(const vector<u8>& data, const usize start, const usize len) noexcept
{
    return std::string_view{reinterpret_cast<const char*>(data.data() + start.get()), len.get()};  //NOLINT
}

std::string_view make_pak_str_zero_padded(const vector<u8>& data, const usize start, const usize max_len) noexcept
{
    usize len;
    for(; len < max_len; ++len) {
        if(data[start + len] == 0) { return make_pak_str(data, start, len + 1_usize); }
    }

    return make_pak_str(data, start, len);
}

} // namespace

void gamepak::load(const fs::path& path)
{
    pak_data_ = fs::read_file(path);
    path_ = path;
    loaded_ = true;

    game_title_ = make_pak_str_zero_padded(pak_data_, 0xA0_usize, 12_usize);
    game_code_ = make_pak_str_zero_padded(pak_data_, 0xAC_usize, 4_usize);
    maker_code_ = make_pak_str_zero_padded(pak_data_, 0xB0_usize, 2_usize);

    main_unit_code_ = pak_data_[0xB3_u32];
    software_version_ = pak_data_[0xBC_u32];
    checksum_ = pak_data_[0xBD_u32];

    u8 calculated_checksum;
    for(u32 addr = 0xA0_u32; addr < 0xBD_u32; ++addr) {
        calculated_checksum -= pak_data_[addr];
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
        LOG_WARN("checksum: {:02X} - mismatch, found: {:02X}", calculated_checksum, checksum_);
    } else {
        LOG_INFO("checksum: {:02X}", calculated_checksum);
    }

    LOG_INFO("---------------------");

    on_load_(path);
}

} // namespace gba
