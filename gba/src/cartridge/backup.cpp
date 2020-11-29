/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cartridge/backup.h>

namespace gba {

void backup_eeprom::write(const u32 address, const u8 value) noexcept
{

}

u8 backup_eeprom::read(const u32 address) const noexcept
{
    return 0_u8;
}

void backup_sram::write(const u32 address, const u8 value) noexcept
{
    data()[address & 0x7FFF_u32] = value;
}

u8 backup_sram::read(const u32 address) const noexcept
{
    return data()[address & 0x7FFF_u32];
}

void backup_flash::write(const u32 address, const u8 value) noexcept
{
    constexpr u32 cmd_addr1 = 0x0E005555_u32;
    constexpr u32 cmd_addr2 = 0x0E002AAA_u32;

    constexpr u8 cmd_devid_start = 0x90_u8;
    constexpr u8 cmd_devid_end = 0xF0_u8;
    constexpr u8 cmd_erase = 0x80_u8;
    constexpr u8 cmd_erase_chip = 0x10_u8;
    constexpr u8 cmd_erase_sector = 0x30_u8;
    constexpr u8 cmd_write_byte = 0xA0_u8;
    constexpr u8 cmd_select_bank = 0xB0_u8;

    switch(state_) {
        case state::accept_cmd:
            if(address == cmd_addr1 && value == 0xAA_u8) {
                state_ = state::cmd_phase1;
            }
            break;
        case state::cmd_phase1:
            if(address == cmd_addr2 && value == 0x55_u8) {
                state_ = state::cmd_phase2;
            }
            break;
        case state::cmd_phase2:
            if(address == cmd_addr1) {
                state_ = state::accept_cmd;

                if(value == cmd_devid_start) {
                    current_cmds_ |= cmd::device_id;
                } else if(value == cmd_devid_end) {
                    current_cmds_ &= ~cmd::device_id;
                } else if(value == cmd_erase) {
                    current_cmds_ |= cmd::erase;
                } else if(value == cmd_write_byte) {
                    state_ = state::cmd_phase3;
                    current_cmds_ |= cmd::write_byte;
                } else if(value == cmd_select_bank && size() == 128_kb) {
                    state_ = state::cmd_phase3;
                    current_cmds_ |= cmd::select_bank;
                } else if((current_cmds_ & cmd::erase) == cmd::erase && value == cmd_erase_chip) {
                    current_cmds_ &= ~cmd::erase;
                    std::fill(data().begin(), data().end(), 0xFF_u8);
                }
            } else if(
              (address & ~0x0000'F000_u32) == 0x0E00'0000_u32 && // 0x0E00'n000 -> erase 4kb sector n
              (current_cmds_ & cmd::erase) == cmd::erase &&
              value == cmd_erase_sector) {
                current_cmds_ &= ~cmd::erase;
                std::fill_n(data().begin() + physical_addr(address & 0xF000_u32).get(), (4_kb).get(), 0xFF_u8);
            }
            break;
        case state::cmd_phase3:
            state_ = state::accept_cmd;

            if((current_cmds_ & cmd::select_bank) == cmd::select_bank && address == 0x0E00'0000_u32) {
                current_bank_ = value & 0x1_u8;
                current_cmds_ &= ~cmd::select_bank;
            } else {
                data()[physical_addr(address & 0xFFFF_u32)] = value;
                current_cmds_ &= ~cmd::write_byte;
            }
            break;
        default:
            UNREACHABLE();
    }
}

u8 backup_flash::read(u32 address) const noexcept
{
    address &= 0xFFFF_u32;
    if((current_cmds_ & cmd::device_id) == cmd::device_id && address < 2_u32) {
        return device_id_[address];
    }

    return data()[physical_addr(address)];
}

} // namespace gba
