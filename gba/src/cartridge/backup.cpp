#include <gba/cartridge/backup.h>

namespace gba {

void backup_eeprom::write(const u32 address, const u8 value) noexcept
{

}

u8 backup_eeprom::read(const u32 address) const noexcept
{
    return 0_u8;
}

void backup_flash::write(const u32 address, const u8 value) noexcept
{

}

u8 backup_flash::read(const u32 address) const noexcept
{
    return 0_u8;
}

} // namespace gba
