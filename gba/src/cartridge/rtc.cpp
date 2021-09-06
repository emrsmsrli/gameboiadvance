/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cartridge/rtc.h>

#define _CRT_SECURE_NO_WARNINGS
#include <chrono>
#include <ctime>

namespace gba::cartridge {

namespace {

constexpr u8 bit_sck = 0_u8;
constexpr u8 bit_sio = 1_u8;
constexpr u8 bit_cs = 2_u8;

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

} // namespace

u8 gpio::read(const u32 address) noexcept
{
    switch(address.get()) {
        case port_data: {
            const u8 new_states = read_pin_states() & (~directions_ & 0xF_u8);
            pin_states_ &= directions_;
            pin_states_ |= new_states;
            return new_states;
        }
        case port_direction: return ~directions_ & 0xF_u8;
        case port_control: return 1_u8; // already handled in io call
        default: return 0_u8;
    }
}

void gpio::write(const u32 address, const u8 value) noexcept
{
    switch(address.get()) {
        case port_data:
            pin_states_ &= ~directions_ & 0xF_u8;
            pin_states_ |= value & directions_;
            write_pin_states(pin_states_);
            break;
        case port_direction:
            directions_ = value;
            break;
        case port_control:
            read_allowed_ = bit::test(value, 0_u8);
            break;
        default:
            break;
    }
}

u8 rtc::read_pin_states() const noexcept
{
    if(state_ == state::sending) {
        return ports_.sio << bit_sio;
    }

    return 1_u8;
}

void rtc::write_pin_states(const u8 new_states) noexcept
{
    const u8 dirs = directions();
    const u8 old_sck = ports_.sck;
    const u8 old_cs  = ports_.cs;

    if(bit::test(dirs, bit_cs)) {
        ports_.cs = bit::extract(new_states, bit_cs);
    }

    if(bit::test(dirs, bit_sck)) {
        ports_.sck = bit::extract(new_states, bit_sck);
    }

    if(bit::test(dirs, bit_sio)) {
        ports_.sio = bit::extract(new_states, bit_sio);
    }

    if(old_cs == 0_u8 && ports_.cs == 1_u8) {
        state_ = state::command;
        current_bit_ = 0_u8;
        current_byte_ = 0_u8;
    }

    if(ports_.cs == 0_u8 || !(old_sck == 0_u8 && ports_.sck == 1_u8)) {
        return;
    }

    switch(state_) {
        case state::command:    sio_receive_cmd(); break;
        case state::receiving:  sio_receive_buffer(); break;
        case state::sending:    sio_send_buffer(); break;
        default: UNREACHABLE();
    }
}

void rtc::sio_receive_cmd() noexcept
{
    if(const bool completed = read_sio(); !completed) {
        return;
    }

    if((current_bit_ >> 4_u8) == 0b0110_u8) {
        bit_buffer_ = (bit_buffer_ << 4_u8) | (bit_buffer_ >> 4_u8);
        bit_buffer_ = ((bit_buffer_ & 0x33_u8) << 2_u8) | ((bit_buffer_ & 0xCC_u8) >> 2_u8);
        bit_buffer_ = ((bit_buffer_ & 0x55_u8) << 1_u8) | ((bit_buffer_ & 0xAA_u8) >> 1_u8);
        LOG_DEBUG(rtc, "received reversed cmd: {:02X}", bit_buffer_);
    } else if((bit_buffer_ & 15_u8) != 6_u8) {
        LOG_DEBUG(rtc, "unknown cmd: {:02X}", bit_buffer_);
        return;
    }

    current_cmd_ = rtc_command{bit_buffer_};
    current_bit_ = 0_u8;
    current_byte_ = 0_u8;
    LOG_DEBUG(rtc, "received cmd: {}", to_string_view(current_cmd_.cmd_type));

    if(current_cmd_.is_access_read) {
        read_register();

        if(parameter_bytes[from_enum<u32>(current_cmd_.cmd_type)] > 0) {
            state_ = state::sending;
        } else {
            state_ = state::command;
        }
    } else {
        if(parameter_bytes[from_enum<u32>(current_cmd_.cmd_type)] > 0) {
            state_ = state::receiving;
        } else {
            write_register();
            state_ = state::command;
        }
    }
}

void rtc::sio_receive_buffer() noexcept
{
    if(current_byte_ < parameter_bytes[from_enum<u32>(current_cmd_.cmd_type)] && read_sio()) {
        internal_regs_[current_byte_] = bit_buffer_;

        if(++current_byte_ == parameter_bytes[from_enum<u32>(current_cmd_.cmd_type)]) {
            write_register();
            state_ = state::command;
        }
    }
}

void rtc::sio_send_buffer() noexcept
{
    ports_.sio = bit::extract(internal_regs_[current_byte_], 0_u8);
    internal_regs_[current_byte_] >>= 1_u8;

    if(++current_bit_ == 8_u8) {
        current_bit_ = 0_u8;
        if(++current_byte_ == parameter_bytes[from_enum<u32>(current_cmd_.cmd_type)]) {
            state_ = state::command;
        }
    }
}

bool rtc::read_sio() noexcept
{
    bit_buffer_ = bit::clear(bit_buffer_, current_bit_);
    bit_buffer_ |= ports_.sio << current_bit_;

    if(++current_bit_ == 8_u8) {
        current_bit_ = 0_u8;
        return true;
    }

    return false;
}

void rtc::read_register() noexcept
{
    using namespace std::chrono;

    const auto to_bcd = [](u8 data) noexcept {
        const u8 counter = data % 10_u8;
        data /= 10_u8;
        return counter + ((data % 10_u8) << 4_u8);
    };

    const auto get_time = [&](u8* ptr, const std::tm* local_time, bool hour24_mode) noexcept {
        if(hour24_mode) {
            *ptr = to_bcd(static_cast<u8::type>(local_time->tm_hour));
        } else {
            *ptr = to_bcd(static_cast<u8::type>(local_time->tm_hour % 12));
        }
        *(ptr + 1) = to_bcd(static_cast<u8::type>(local_time->tm_min));
        *(ptr + 2) = to_bcd(static_cast<u8::type>(local_time->tm_sec));
    };

    switch(current_cmd_.cmd_type) {
        case rtc_command::type::time: {
            const std::time_t t = system_clock::to_time_t(system_clock::now());
            const std::tm* local_time = std::localtime(&t);
            get_time(internal_regs_.ptr(0_u32), local_time, bit::test(control_, 6_u8));
            break;
        }
        case rtc_command::type::date_time: {
            const std::time_t t = system_clock::to_time_t(system_clock::now());
            const std::tm* local_time = std::localtime(&t);
            internal_regs_[0_u32] = to_bcd(static_cast<u8::type>(local_time->tm_year - 100));
            internal_regs_[1_u32] = to_bcd(static_cast<u8::type>(local_time->tm_mon + 1));
            internal_regs_[2_u32] = to_bcd(static_cast<u8::type>(local_time->tm_mday));
            internal_regs_[3_u32] = to_bcd(static_cast<u8::type>(local_time->tm_wday));
            get_time(internal_regs_.ptr(4_u32), local_time, bit::test(control_, 6_u8));
            break;
        }
        case rtc_command::type::control:
            internal_regs_[0_u32] = control_;
            break;
        case rtc_command::type::force_irq:
        case rtc_command::type::reset:
        case rtc_command::type::none:
        case rtc_command::type::free:
            break;
    }
}

void rtc::write_register() noexcept
{
    switch(current_cmd_.cmd_type) {
        case rtc_command::type::none:
        case rtc_command::type::date_time:
        case rtc_command::type::free:
        case rtc_command::type::time:
            break;
        case rtc_command::type::force_irq:
            irq_.request_interrupt(cpu::interrupt_source::gamepak);
            break;
        case rtc_command::type::reset:
            control_ = 0_u8;
            break;
        case rtc_command::type::control:
            control_ = bit_buffer_;
            break;
        default:
            break;
    }
}

} // namespace gba::cartridge
