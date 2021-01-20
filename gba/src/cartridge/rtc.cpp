/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cartridge/rtc.h>

#include <string_view>
#include <chrono>
#include <ctime>

namespace gba {

namespace {

constexpr u8 bit_sck = 0_u8;
constexpr u8 bit_sio = 1_u8;
constexpr u8 bit_cs = 2_u8;
constexpr u8 pin_sck = 1_u8 << bit_sck;
constexpr u8 pin_cs = 1_u8 << bit_cs;

constexpr array<u8, 8> parameter_bytes{
  0_u8, // reset
  0_u8, // -
  7_u8, // set_date_time
  0_u8, // force_irq
  1_u8, // set_control
  0_u8, // -
  3_u8, // set_time
  0_u8  // -
};

std::string_view to_string_view(const rtc_command::type type) {
    switch(type) {
        case rtc_command::type::none: return "none";
        case rtc_command::type::reset: return "reset";
        case rtc_command::type::date_time: return "set_date_time";
        case rtc_command::type::force_irq: return "force_irq";
        case rtc_command::type::control: return "set_control";
        case rtc_command::type::time: return "set_time";
        default:
            UNREACHABLE();
    }
}

void get_time(array<u8, 7>& container, bool hour24_mode) noexcept
{
    const auto to_bcd = [](u8 data) {
        const u8 counter = data % 10_u8;
        data /= 10_u8;
        return counter + ((data % 10_u8) << 4_u8);
    };

    using namespace std::chrono;
    const std::time_t t = system_clock::to_time_t(system_clock::now());
    const std::tm* date = std::localtime(&t);

    container[0_usize] = to_bcd(static_cast<u8::type>(date->tm_year - 100));
    container[1_usize] = to_bcd(static_cast<u8::type>(date->tm_mon + 1));
    container[2_usize] = to_bcd(static_cast<u8::type>(date->tm_mday));
    container[3_usize] = to_bcd(static_cast<u8::type>(date->tm_wday));
    if(hour24_mode) {
        container[4_usize] = to_bcd(static_cast<u8::type>(date->tm_hour));
    } else {
        container[4_usize] = to_bcd(static_cast<u8::type>(date->tm_hour % 12));
    }
    container[5_usize] = to_bcd(static_cast<u8::type>(date->tm_min));
    container[6_usize] = to_bcd(static_cast<u8::type>(date->tm_sec));
}

} // namespace

u8 gba::rtc::read(const u32 address) const noexcept
{
    if(!read_allowed_) { return 0x0_u8; }

    switch(address.get()) {
        case port_data: return pin_states_;
        case port_direction: return directions_;
        case port_control: return 1_u8;
        default:
            UNREACHABLE();
    }
}

void rtc::write(const u32 address, const u8 value) noexcept
{
    switch(address.get()) {
        case port_data:
            pin_states_ &= ~directions_ & 0xF_u8;
            pin_states_ |= value & directions_;
            read_pins();
            break;
        case port_direction:
            directions_ = value;
            break;
        case port_control:
            read_allowed_ = bit::test(value, 0_u8);
            break;
        default:
            UNREACHABLE();
    }
}

void rtc::read_pins() noexcept
{
    switch(transfer_state_) {
        case transfer_state::waiting_hi_sck: {
            if((pin_states_ & (pin_sck | pin_cs)) == pin_sck) {
                transfer_state_ = transfer_state::waiting_hi_cs;
            }
            break;
        }
        case transfer_state::waiting_hi_cs: {
            if((pin_states_ & (pin_sck | pin_cs)) == (pin_sck | pin_cs)) {
                transfer_state_ = transfer_state::transferring_cmd;
            } else {
                transfer_state_ = transfer_state::waiting_hi_sck;
            }
            break;
        }
        case transfer_state::transferring_cmd: {
            // .. b2  | b1  | b0
            // .. cs  | sio | sck
            // 1. 0   | x   | 1  init
            // 2. 1   | x   | 1  start accepting cmd
            // 3. 1   | D   | 0  read data at low sck
            // 4. 1   | x   | 1  complete bit read, repeat 3-4 8 times
            // 5. 0   | x   | x  end transfer

            if(!bit::test(pin_states_, bit_sck)) {
                // data is available on sck falling edge, it is written when sck goes high
                bit_buffer_ = bit::clear(bit_buffer_, bits_read_);
                bit_buffer_ |= bit::extract(pin_states_, bit_sio) << bits_read_;
            } else if(bit::test(pin_states_, bit_cs)) {
                // either writing to rtc registers, or transferring cmd
                if(!current_cmd_.is_access_read) {
                    ++bits_read_;
                    if(bits_read_ == 8_u8) {
                        process_byte();
                    }
                } else {
                    write_pins(pin_sck | pin_cs | (get_output_byte() << 1_u8));
                    ++bits_read_;
                    if(bits_read_ == 8_u8) {
                        bits_read_ = 0_u8;
                        --remaining_bytes_;
                        if(remaining_bytes_ == 0_u8) {
                            current_cmd_.cmd_type = rtc_command::type::none;
                        }
                    }
                }
            } else { // cs is reset to 0
                bits_read_ = 0_u8;
                remaining_bytes_ = 0_u8;
                current_cmd_.cmd_type = rtc_command::type::none;
                transfer_state_ = static_cast<transfer_state>(bit::extract(pin_states_, 0_u8).get());
                write_pins(pin_sck);
            }
            break;
        }
    }
}

void rtc::write_pins(const u8 new_states) noexcept
{
    if(read_allowed_) {
        pin_states_ &= directions_;
        pin_states_ |= new_states & ~directions_ & 0xF_u8;
    }
}

void rtc::process_byte() noexcept
{
    --remaining_bytes_;
    if(current_cmd_.cmd_type == rtc_command::type::none) {
        if((bit_buffer_ & 0xF_u8) == 0b0110_u8) {
            current_cmd_ = rtc_command{bit_buffer_};
            remaining_bytes_ = parameter_bytes[static_cast<u8::type>(current_cmd_.cmd_type)];
            LOG_TRACE("RTC cmd: {}", to_string_view(current_cmd_.type));

            switch(current_cmd_.cmd_type) {
                case rtc_command::type::reset:
                    // no-op since we update the clock based on system clock
                case rtc_command::type::force_irq:
                    // TODO IRQ to cpu
                    break;
                case rtc_command::type::control:
                    control_ = 0_u8;
                    break;
                case rtc_command::type::time:
                case rtc_command::type::date_time:
                    get_time(time_regs_, bit::test(control_, 6_u8));
                    break;
                case rtc_command::type::none:
                    break;
            }
        } else {
            LOG_WARN("invalid RTC cmd: {:02X}", bit_buffer_);
        }
    } else {
        switch(current_cmd_.cmd_type) {
            case rtc_command::type::none:
            case rtc_command::type::reset:
            case rtc_command::type::date_time:
            case rtc_command::type::force_irq:
            case rtc_command::type::time:
                break;
            case rtc_command::type::control:
                control_ = bit_buffer_;
            default:
                break;
        }
    }

    bit_buffer_ = 0_u8;
    bits_read_ = 0_u8;
    if(remaining_bytes_ == 0_u8) {
        current_cmd_.cmd_type = rtc_command::type::none;
    }
}

u8 rtc::get_output_byte() noexcept
{
    if (current_cmd_.cmd_type == rtc_command::type::none) {
        LOG_ERROR("reading RTC without cmd");
        return 0_u8;
    }

    switch(current_cmd_.cmd_type) {
        case rtc_command::type::control:
            return bit::extract(control_, bits_read_);
        case rtc_command::type::time:
        case rtc_command::type::date_time:
            return bit::extract(time_regs_[7_u8 - remaining_bytes_], bits_read_);
        case rtc_command::type::none:
        case rtc_command::type::reset:
        case rtc_command::type::force_irq:
            return 0_u8;
        default:
            UNREACHABLE();
    }
}

} // namespace gba
