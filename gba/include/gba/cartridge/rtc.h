/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_RTC_H
#define GAMEBOIADVANCE_RTC_H

#include <gba/cpu/irq_controller_handle.h>
#include <gba/core/container.h>
#include <gba/core/math.h>

namespace gba::cartridge {

class gpio {
    u8 pin_states_;
    u8 directions_ = 0xF_u8; // first 4 bits are used, 0 -> in(to gba), 1 -> out(to device)
    bool read_allowed_ = false;

public:
    static inline constexpr auto port_data = 0xC4_u32;
    static inline constexpr auto port_direction = 0xC6_u32;
    static inline constexpr auto port_control = 0xC8_u32;

    virtual ~gpio() = default;

    [[nodiscard]] u8 read(u32 address) noexcept;
    void write(u32 address, u8 value) noexcept;

    [[nodiscard]] bool read_allowed() const noexcept { return read_allowed_; }

protected:
    [[nodiscard]] u8 directions() const noexcept { return directions_; }

    [[nodiscard]] virtual u8 read_pin_states() const noexcept = 0;
    virtual void write_pin_states(u8 new_states) noexcept = 0;
};

struct rtc_command  {
    enum class type : u8::type {
        none = 0b0001'0000_u16,
        reset = 0b0000_u16,
        date_time = 0b0010_u16,
        force_irq = 0b0011_u16,
        control = 0b0100_u16,
        time = 0b0110_u16,
        free = 0b0111_u16,
    };

    type cmd_type{type::none};
    bool is_access_read = false;

    rtc_command() = default;
    explicit rtc_command(u8 cmd)
    {
        cmd_type = to_enum<type>((cmd >> 4_u8) & 0x7_u8);
        is_access_read = bit::test(cmd, 7_u8);
    }
};

constexpr std::string_view to_string_view(const rtc_command::type type) {
    switch(type) {
        case rtc_command::type::none: return "none";
        case rtc_command::type::reset: return "reset";
        case rtc_command::type::date_time: return "set_date_time";
        case rtc_command::type::force_irq: return "force_irq";
        case rtc_command::type::control: return "set_control";
        case rtc_command::type::time: return "set_time";
        case rtc_command::type::free: return "free";
        default:
            UNREACHABLE();
    }
}

struct rtc_ports {
    u8 sck;
    u8 sio;
    u8 cs;
};

class rtc : public gpio {
    enum class state : u8::type {
        command,
        sending,
        receiving
    };

    cpu::irq_controller_handle irq_;

    array<u8, 7> internal_regs_;
    u8 control_;

    state state_{state::command};
    rtc_command current_cmd_;
    u8 current_byte_;
    u8 current_bit_;
    u8 bit_buffer_;

    rtc_ports ports_;

public:
#if WITH_DEBUGGER
    using state_debugger = state;
#endif // WITH_DEBUGGER

    void set_irq_controller_handle(const cpu::irq_controller_handle irq) noexcept { irq_ = irq; }

protected:
    [[nodiscard]] u8 read_pin_states() const noexcept final;
    void write_pin_states(u8 new_states) noexcept final;

private:
    void sio_receive_cmd() noexcept;
    void sio_receive_buffer() noexcept;
    void sio_send_buffer() noexcept;

    bool read_sio() noexcept;
    void read_register() noexcept;
    void write_register() noexcept;
};

} // namespace gba::cartridge

#endif //GAMEBOIADVANCE_RTC_H
