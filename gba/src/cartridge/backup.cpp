/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cartridge/backup.h>

#include <gba/core/scheduler.h>
#include <gba/core/math.h>
#include <gba/helper/bitflags.h>

ENABLE_BITFLAG_OPS(gba::cartridge::backup_flash::cmd);

namespace gba::cartridge {

namespace {

// from mgba
constexpr u64 eeprom_settle_cycles = 115'000_u64;

} // namespace

void backup::set_size(const usize size) noexcept
{
    size_ = size;
    data_.resize(size);
}

void backup_eeprom::write(const u32 /*address*/, u8 value) noexcept
{
    constexpr u64 read_request_pattern = 0b11_u64;
    constexpr u64 write_request_pattern = 0b10_u64;

    value &= 0x1_u8;
    buffer_ = (buffer_ << 1_u8) | value;
    ++transmission_count_;

    switch(state_) {
        case state::accepting_commands: {
            if(transmission_count_ == 2_u8) {
                if(buffer_ == write_request_pattern) {
                    state_ = state::transmitting_addr;
                    cmd_ = cmd::write;
                } else if(buffer_ == read_request_pattern) {
                    state_ = state::transmitting_addr;
                    cmd_ = cmd::read;
                }

                LOG_TRACE(eeprom, "new state: transmitting_addr, read mode: {}", read_mode_);
                reset_buffer();
            }
            break;
        }
        case state::transmitting_addr: {
            if(transmission_count_ == bus_width_) {
                address_ = narrow<u32>(buffer_ * 8_u32) & 0x3FF_u32;  // addressing works in 64bit units
                reset_buffer();

                ASSERT(cmd_ != cmd::none);

                // write request automatically erases 64 bits of data
                if(cmd_ == cmd::write) {
                    memcpy(data(), address_, 0_u64);
                    state_ = state::transmitting_data;
                    LOG_TRACE(eeprom, "new state: transmitting_data in write mode, erasing {:08X}", address_);
                } else {
                    state_ = state::waiting_finish_bit;
                    LOG_TRACE(eeprom, "new state: waiting_finish_bit in read mode");
                }
            }
            break;
        }
        case state::transmitting_data: {
            if(cmd_ == cmd::write && transmission_count_ == 64_u8) {
                memcpy(data(), address_, buffer_);
                reset_buffer();
                state_ = state::waiting_finish_bit;
                LOG_TRACE(eeprom, "new state: waiting_finish_bit after transmitting");

                settled_response_ = 0_u8;
                scheduler_->ADD_HW_EVENT(eeprom_settle_cycles, backup_eeprom::on_settle);
            }
            break;
        }
        case state::waiting_finish_bit: {
            if(cmd_ == cmd::write) {
                state_ = state::accepting_commands;
                cmd_ = cmd::none;
                LOG_TRACE(eeprom, "new state: accepting_commands");
            } else {
                state_ = state::transmitting_ignored_bits;
                LOG_TRACE(eeprom, "new state: transmitting_ignored_bits");
            }

            reset_buffer();
            break;
        }
        case state::transmitting_ignored_bits:
            break;
    }
}

u8 backup_eeprom::read(const u32 /*address*/) const noexcept
{
    if(cmd_ == cmd::read) {
        if(state_ == state::transmitting_ignored_bits) {
            ++transmission_count_;
            if(transmission_count_ == 4) {
                state_ = state::transmitting_data;
                LOG_TRACE(eeprom, "new state: transmitting_data");
                transmission_count_ = 0_u8;
                buffer_ = memcpy<u64>(data(), address_);
            }

            return 0_u8;
        }

        if(state_ == state::transmitting_data) {
            const u8 data = narrow<u8>( bit::extract(buffer_, 63_u8 - transmission_count_));
            ++transmission_count_;

            if(transmission_count_ == 64_u8) {
                state_ = state::accepting_commands;
                cmd_ = cmd::none;
                LOG_TRACE(eeprom, "new state: accepting_commands");
                reset_buffer();
            }

            return data;
        }
    }

    return settled_response_;
}

void backup_eeprom::set_size(const usize size) noexcept
{
    backup::set_size(size);
    bus_width_ = size == 8_kb ? 14_u8 : 6_u8;
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
            state_ = state::accept_cmd;

            if(address == cmd_addr1) {
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
                } else if(bitflags::is_set(current_cmds_, cmd::erase) && value == cmd_erase_chip) {
                    current_cmds_ &= ~cmd::erase;
                    std::fill(data().begin(), data().end(), 0xFF_u8);
                    LOG_TRACE(flash, "erasing chip");
                }
            } else if(
              (address & ~0x0000'F000_u32) == 0x0E00'0000_u32 && // 0x0E00'n000 -> erase 4kb sector n
              bitflags::is_set(current_cmds_, cmd::erase) &&
              value == cmd_erase_sector) {
                current_cmds_ &= ~cmd::erase;
                std::fill_n(data().begin() + physical_addr(address & 0xF000_u32).get(), (4_kb).get(), 0xFF_u8);
                LOG_TRACE(flash, "erasing sector {:X}", (address & 0xF000_u32) >> 12_u32);
            }
            break;
        case state::cmd_phase3:
            state_ = state::accept_cmd;

            if(bitflags::is_set(current_cmds_, cmd::select_bank) && address == 0x0E00'0000_u32) {
                current_bank_ = bit::extract(value, 0_u8);
                current_cmds_ &= ~cmd::select_bank;
                LOG_TRACE(flash, "selecting bank {}", current_bank_);
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
    if(bitflags::is_set(current_cmds_, cmd::device_id) && address < 2_u32) {
        return device_id_[address];
    }

    return data()[physical_addr(address)];
}

} // namespace gba::cartridge
