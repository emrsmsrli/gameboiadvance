/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_RTC_H
#define GAMEBOIADVANCE_RTC_H

#include <gba/core/container.h>
#include <gba/core/math.h>
#include <gba/arm/irq_controller_handle.h>

namespace gba::cartridge {

struct rtc_command  {
    enum class type : u8::type {
        none = 0b0001'0000_u16,
        reset = 0b0000_u16,
        date_time = 0b0010_u16,
        force_irq = 0b0011_u16,
        control = 0b0100_u16,
        time = 0b0110_u16,
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

class rtc {
    // gpio data
    u8 pin_states_;
    u8 directions_; // first 4 bits are used, 0 -> in(to gba), 1 -> out(to device)
    u8 remaining_bytes_;
    u8 bits_read_;
    u8 bit_buffer_;
    bool read_allowed_ = false;

    enum class transfer_state : u8::type { waiting_hi_sck, waiting_hi_cs, transferring_cmd };
    transfer_state transfer_state_{transfer_state::waiting_hi_sck};

    rtc_command current_cmd_;
    array<u8, 7> time_regs_;
    u8 control_ = 0x40_u8;

    arm::irq_controller_handle irq_;

public:
#if WITH_DEBUGGER
    using transfer_state_debugger = transfer_state;
    using time_regs_debugger = array<u8, 7>;
  #endif // WITH_DEBUGGER

    static inline constexpr auto port_data = 0xC4_u32;
    static inline constexpr auto port_direction = 0xC6_u32;
    static inline constexpr auto port_control = 0xC8_u32;

    [[nodiscard]] u8 read(u32 address) const noexcept;
    void write(u32 address, u8 value) noexcept;

    [[nodiscard]] bool read_allowed() const noexcept { return read_allowed_; }

    void set_irq_controller_handle(const arm::irq_controller_handle irq) noexcept { irq_ = irq; }

private:
    void read_pins() noexcept;
    void write_pins(u8 new_states) noexcept;
    void process_byte() noexcept;
    [[nodiscard]] u8 get_output_byte() noexcept;
};

} // namespace gba::cartridge

#endif //GAMEBOIADVANCE_RTC_H
