#include <gba/cartridge/gamepak.h>
#include <gba/cartridge/gamepak_db.h>

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

std::unique_ptr<backup> make_backup_from_type(const backup::type type, const fs::path& pak_path)
{
    switch(type) {
        case backup::type::none:
            return nullptr;
        case backup::type::eeprom_4:
            return std::make_unique<backup_eeprom>(pak_path, 512_usize);
        case backup::type::eeprom_64:
            return std::make_unique<backup_eeprom>(pak_path, 8_kb);
        case backup::type::sram:
            return std::make_unique<backup_sram>(pak_path);
        case backup::type::flash_64:
            return std::make_unique<backup_flash>(pak_path, 64_kb);
        case backup::type::flash_128:
            return std::make_unique<backup_flash>(pak_path, 128_kb);
        default:
            UNREACHABLE();
    }
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

    detect_backup_type();

    LOG_INFO("rtc: {}", has_rtc_);
    LOG_INFO("address mirroring: {}", has_mirroring_);

    LOG_INFO("---------------------");

    on_load_(path);
}

void gamepak::detect_backup_type() noexcept
{
    const std::optional<pak_db_entry> entry = query_pak_db(game_code_);

    if(entry.has_value() && entry->backup_type != backup::type::detect) {
        backup_type_ = entry->backup_type;
        has_mirroring_ = entry->has_mirroring;
        has_rtc_ = entry->has_rtc;

        // todo init rtc

        backup_ = make_backup_from_type(backup_type_, path_);
        LOG_INFO("backup: {} (database entry)", to_string_view(backup_type_));
        return;
    }

    using namespace std::string_view_literals;
    static constexpr array backup_type_strings{
      std::make_pair("EEPROM_V"sv, backup::type::eeprom_64),
      std::make_pair("SRAM_V"sv, backup::type::sram),
      std::make_pair("SRAM_F_V"sv, backup::type::sram),
      std::make_pair("FLASH_V"sv, backup::type::flash_64),
      std::make_pair("FLASH512_V"sv, backup::type::flash_64),
      std::make_pair("FLASH1M_V"sv, backup::type::flash_128),
    };

    std::string_view pak_str = make_pak_str(pak_data_, 0_usize, pak_data_.size());
    for(const auto& [name, type] : backup_type_strings) {
        if(pak_str.find(name) != std::string_view::npos) {
            backup_type_ = type;
            backup_ = make_backup_from_type(backup_type_, path_);
        }
    }

    if(backup_type_ == backup::type::detect) {
        backup_type_ = backup::type::sram;
        backup_ = std::make_unique<backup_sram>(path_);
        LOG_WARN("backup: {} (fallback)", to_string_view(backup_type_));
    } else {
        LOG_INFO("backup: {}", to_string_view(backup_type_));
    }
}

} // namespace gba
