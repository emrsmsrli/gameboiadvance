/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/core.h>

namespace gba {

u8 core::read_io(const u32 addr, const cpu::mem_access access) noexcept
{
    auto& timer_controller = cpu_.timer_controller_;
    auto& dma_controller = cpu_.dma_controller_;

    const auto win_enable_read = [](ppu::win_enable_bits& area) {
        return bit::from_bool<u8>(area.bg_enabled[0_usize])
          | bit::from_bool<u8>(area.bg_enabled[1_usize]) << 1_u8
          | bit::from_bool<u8>(area.bg_enabled[2_usize]) << 2_u8
          | bit::from_bool<u8>(area.bg_enabled[3_usize]) << 3_u8
          | bit::from_bool<u8>(area.obj_enabled) << 4_u8
          | bit::from_bool<u8>(area.blend_enabled) << 5_u8;
    };

    switch(addr.get()) {
        case keypad::addr_state:     return narrow<u8>(keypad_.keyinput_);
        case keypad::addr_state + 1: return narrow<u8>(keypad_.keyinput_ >> 8_u16);
        case keypad::addr_control:   return narrow<u8>(keypad_.keycnt_.select);
        case keypad::addr_control + 1:
            return narrow<u8>(((widen<u32>(keypad_.keycnt_.select) >> 8_u32) & 0b11_u32)
              | bit::from_bool(keypad_.keycnt_.enabled) << 6_u32
              | from_enum<u32>(keypad_.keycnt_.cond_strategy) << 7_u32);

        case ppu::addr_dispcnt:
            return bit::from_bool<u8>(ppu_engine_.dispcnt_.forced_blank) << 7_u8
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.obj_mapping_1d) << 6_u8
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.hblank_interval_free) << 5_u8
              | ppu_engine_.dispcnt_.frame_select << 4_u8
              | ppu_engine_.dispcnt_.bg_mode;
        case ppu::addr_dispcnt + 1:
            return bit::from_bool<u8>(ppu_engine_.dispcnt_.bg_enabled[0_usize])
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.bg_enabled[1_usize]) << 1_u8
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.bg_enabled[2_usize]) << 2_u8
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.bg_enabled[3_usize]) << 3_u8
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.obj_enabled) << 4_u8
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.win0_enabled) << 5_u8
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.win1_enabled) << 6_u8
              | bit::from_bool<u8>(ppu_engine_.dispcnt_.win_obj_enabled) << 7_u8;
        case ppu::addr_greenswap:     return bit::from_bool<u8>(ppu_engine_.green_swap_);
        case ppu::addr_greenswap + 1: return 0_u8;
        case ppu::addr_dispstat:
            return bit::from_bool<u8>(ppu_engine_.dispstat_.vblank)
              | bit::from_bool<u8>(ppu_engine_.dispstat_.hblank) << 1_u8
              | bit::from_bool<u8>(ppu_engine_.dispstat_.vcounter) << 2_u8
              | bit::from_bool<u8>(ppu_engine_.dispstat_.vblank_irq_enabled) << 3_u8
              | bit::from_bool<u8>(ppu_engine_.dispstat_.hblank_irq_enabled) << 4_u8
              | bit::from_bool<u8>(ppu_engine_.dispstat_.vcounter_irq_enabled) << 5_u8;
        case ppu::addr_dispstat + 1: return ppu_engine_.dispstat_.vcount_setting;
        case ppu::addr_vcount:       return ppu_engine_.vcount_;
        case ppu::addr_vcount + 1:   return 0_u8;
        case ppu::addr_bg0cnt:       return ppu_engine_.bg0_.cnt.read_lower();
        case ppu::addr_bg0cnt + 1:   return ppu_engine_.bg0_.cnt.read_upper();
        case ppu::addr_bg1cnt:       return ppu_engine_.bg1_.cnt.read_lower();
        case ppu::addr_bg1cnt + 1:   return ppu_engine_.bg1_.cnt.read_upper();
        case ppu::addr_bg2cnt:       return ppu_engine_.bg2_.cnt.read_lower();
        case ppu::addr_bg2cnt + 1:   return ppu_engine_.bg2_.cnt.read_upper();
        case ppu::addr_bg3cnt:       return ppu_engine_.bg3_.cnt.read_lower();
        case ppu::addr_bg3cnt + 1:   return ppu_engine_.bg3_.cnt.read_upper();
        case ppu::addr_winin:        return win_enable_read(ppu_engine_.win_in_.win0);
        case ppu::addr_winin + 1:    return win_enable_read(ppu_engine_.win_in_.win1);
        case ppu::addr_winout:       return win_enable_read(ppu_engine_.win_out_.outside);
        case ppu::addr_winout + 1:   return win_enable_read(ppu_engine_.win_out_.obj);
        case ppu::addr_bldcnt:
            return bit::from_bool<u8>(ppu_engine_.bldcnt_.first.bg[0_usize])
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.first.bg[1_usize]) << 1_u8
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.first.bg[2_usize]) << 2_u8
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.first.bg[3_usize]) << 3_u8
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.first.obj) << 4_u8
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.first.backdrop) << 5_u8
              | from_enum<u8>(ppu_engine_.bldcnt_.type) << 6_u8;
        case ppu::addr_bldcnt + 1:
            return bit::from_bool<u8>(ppu_engine_.bldcnt_.second.bg[0_usize])
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.second.bg[1_usize]) << 1_u8
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.second.bg[2_usize]) << 2_u8
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.second.bg[3_usize]) << 3_u8
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.second.obj) << 4_u8
              | bit::from_bool<u8>(ppu_engine_.bldcnt_.second.backdrop) << 5_u8;
        case ppu::addr_bldalpha:     return ppu_engine_.blend_settings_.eva;
        case ppu::addr_bldalpha + 1: return ppu_engine_.blend_settings_.evb;

        case apu::addr_sound1cnt_l:     return apu_engine_.channel_1_.swp.read();
        case apu::addr_sound1cnt_l + 1: return 0_u8;
        case apu::addr_sound1cnt_h:     return apu_engine_.channel_1_.wav_data.read();
        case apu::addr_sound1cnt_h + 1: return apu_engine_.channel_1_.env.read();
        case apu::addr_sound1cnt_x:     return 0_u8;
        case apu::addr_sound1cnt_x + 1: return apu_engine_.channel_1_.freq_data.freq_control.read();
        case apu::addr_sound1cnt_x + 2:
        case apu::addr_sound1cnt_x + 3: return 0_u8;
        case apu::addr_sound2cnt_l:     return apu_engine_.channel_2_.wav_data.read();
        case apu::addr_sound2cnt_l + 1: return apu_engine_.channel_2_.env.read();
        case apu::addr_sound2cnt_h:     return 0_u8;
        case apu::addr_sound2cnt_h + 1: return apu_engine_.channel_2_.freq_data.freq_control.read();
        case apu::addr_sound2cnt_h + 2:
        case apu::addr_sound2cnt_h + 3: return 0_u8;
        case apu::addr_sound3cnt_l:
            return bit::from_bool<u8>(apu_engine_.channel_3_.wave_bank_2d) << 5_u8
              | apu_engine_.channel_3_.wave_bank << 6_u8
              | bit::from_bool<u8>(apu_engine_.channel_3_.dac_enabled) << 7_u8;
        case apu::addr_sound3cnt_l + 1:
        case apu::addr_sound3cnt_h:     return 0_u8;
        case apu::addr_sound3cnt_h + 1:
            return apu_engine_.channel_3_.output_level << 5_u8
              | bit::from_bool<u8>(apu_engine_.channel_3_.force_output_level) << 7_u8;
        case apu::addr_sound3cnt_x:     return 0_u8;
        case apu::addr_sound3cnt_x + 1: return apu_engine_.channel_3_.freq_data.freq_control.read();
        case apu::addr_sound3cnt_x + 2:
        case apu::addr_sound3cnt_x + 3:
        case apu::addr_sound4cnt_l:     return 0_u8;
        case apu::addr_sound4cnt_l + 1: return apu_engine_.channel_4_.env.read();
        case apu::addr_sound4cnt_l + 2:
        case apu::addr_sound4cnt_l + 3: return 0_u8;
        case apu::addr_sound4cnt_h:     return apu_engine_.channel_4_.polynomial_cnt.read();
        case apu::addr_sound4cnt_h + 1: return apu_engine_.channel_4_.freq_control.read();
        case apu::addr_sound4cnt_h + 2:
        case apu::addr_sound4cnt_h + 3: return 0_u8;
        case apu::addr_soundcnt_l:      return apu_engine_.control_.read<0>();
        case apu::addr_soundcnt_l + 1:  return apu_engine_.control_.read<1>();
        case apu::addr_soundcnt_h:      return apu_engine_.control_.read<2>();
        case apu::addr_soundcnt_h + 1:  return apu_engine_.control_.read<3>();
        case apu::addr_soundcnt_x:
            return bit::from_bool<u8>(apu_engine_.power_on_) << 7_u8
              | bit::from_bool<u8>(apu_engine_.channel_4_.enabled) << 3_u8
              | bit::from_bool<u8>(apu_engine_.channel_3_.enabled) << 2_u8
              | bit::from_bool<u8>(apu_engine_.channel_2_.enabled) << 1_u8
              | bit::from_bool<u8>(apu_engine_.channel_1_.enabled) << 0_u8;
        case apu::addr_soundcnt_x + 1:
        case apu::addr_soundcnt_x + 2:
        case apu::addr_soundcnt_x + 3:  return 0_u8;
        case apu::addr_soundbias:       return narrow<u8>(apu_engine_.soundbias_.bias);
        case apu::addr_soundbias + 1:   return (narrow<u8>(apu_engine_.soundbias_.bias >> 8_u16) & 0b11_u8) | apu_engine_.soundbias_.resolution << 6_u8;
        case apu::addr_soundbias + 2:
        case apu::addr_soundbias + 3:   return 0_u8;
        case apu::addr_wave_ram:      case apu::addr_wave_ram + 1:  case apu::addr_wave_ram + 2:  case apu::addr_wave_ram + 3:
        case apu::addr_wave_ram + 4:  case apu::addr_wave_ram + 5:  case apu::addr_wave_ram + 6:  case apu::addr_wave_ram + 7:
        case apu::addr_wave_ram + 8:  case apu::addr_wave_ram + 9:  case apu::addr_wave_ram + 10: case apu::addr_wave_ram + 11:
        case apu::addr_wave_ram + 12: case apu::addr_wave_ram + 13: case apu::addr_wave_ram + 14: case apu::addr_wave_ram + 15:
            return apu_engine_.channel_3_.read_wave_ram(addr & 0xF_u32);

        case sio::addr_siomulti0:
        case sio::addr_siomulti0 + 1:
        case sio::addr_siomulti1:
        case sio::addr_siomulti1 + 1:
        case sio::addr_siomulti2:
        case sio::addr_siomulti2 + 1:
        case sio::addr_siomulti3:
        case sio::addr_siomulti3 + 1:
        case sio::addr_siocnt:
        case sio::addr_siocnt + 1:
        case sio::addr_siomlt_send:
        case sio::addr_siomlt_send + 1:
        case sio::addr_rcnt:
            return 0x00_u8;
        case sio::addr_rcnt + 1:
            return 0x80_u8;
        case sio::addr_joycnt:
        case sio::addr_joycnt + 1:
        case sio::addr_joy_recv:
        case sio::addr_joy_recv + 1:
        case sio::addr_joy_trans:
        case sio::addr_joy_trans + 1:
        case sio::addr_joystat:
        case sio::addr_joystat + 1:
            return 0_u8;

        case cpu::addr_tm0cnt_l:     return timer_controller[0_usize].read(timer::register_type::cnt_l_lsb);
        case cpu::addr_tm0cnt_l + 1: return timer_controller[0_usize].read(timer::register_type::cnt_l_msb);
        case cpu::addr_tm0cnt_h:     return timer_controller[0_usize].read(timer::register_type::cnt_h_lsb);
        case cpu::addr_tm0cnt_h + 1: return 0_u8;
        case cpu::addr_tm1cnt_l:     return timer_controller[1_usize].read(timer::register_type::cnt_l_lsb);
        case cpu::addr_tm1cnt_l + 1: return timer_controller[1_usize].read(timer::register_type::cnt_l_msb);
        case cpu::addr_tm1cnt_h:     return timer_controller[1_usize].read(timer::register_type::cnt_h_lsb);
        case cpu::addr_tm1cnt_h + 1: return 0_u8;
        case cpu::addr_tm2cnt_l:     return timer_controller[2_usize].read(timer::register_type::cnt_l_lsb);
        case cpu::addr_tm2cnt_l + 1: return timer_controller[2_usize].read(timer::register_type::cnt_l_msb);
        case cpu::addr_tm2cnt_h:     return timer_controller[2_usize].read(timer::register_type::cnt_h_lsb);
        case cpu::addr_tm2cnt_h + 1: return 0_u8;
        case cpu::addr_tm3cnt_l:     return timer_controller[3_usize].read(timer::register_type::cnt_l_lsb);
        case cpu::addr_tm3cnt_l + 1: return timer_controller[3_usize].read(timer::register_type::cnt_l_msb);
        case cpu::addr_tm3cnt_h:     return timer_controller[3_usize].read(timer::register_type::cnt_h_lsb);
        case cpu::addr_tm3cnt_h + 1:

        case cpu::addr_dma0cnt_l:
        case cpu::addr_dma0cnt_l + 1: return 0_u8;
        case cpu::addr_dma0cnt_h:     return dma_controller[0_usize].read_cnt_l();
        case cpu::addr_dma0cnt_h + 1: return dma_controller[0_usize].read_cnt_h();
        case cpu::addr_dma1cnt_l:
        case cpu::addr_dma1cnt_l + 1: return 0_u8;
        case cpu::addr_dma1cnt_h:     return dma_controller[1_usize].read_cnt_l();
        case cpu::addr_dma1cnt_h + 1: return dma_controller[1_usize].read_cnt_h();
        case cpu::addr_dma2cnt_l:
        case cpu::addr_dma2cnt_l + 1: return 0_u8;
        case cpu::addr_dma2cnt_h:     return dma_controller[2_usize].read_cnt_l();
        case cpu::addr_dma2cnt_h + 1: return dma_controller[2_usize].read_cnt_h();
        case cpu::addr_dma3cnt_l:
        case cpu::addr_dma3cnt_l + 1: return 0_u8;
        case cpu::addr_dma3cnt_h:     return dma_controller[3_usize].read_cnt_l();
        case cpu::addr_dma3cnt_h + 1: return dma_controller[3_usize].read_cnt_h();

        case cpu::addr_ime:     return bit::from_bool<u8>(cpu_.ime_);
        case cpu::addr_ime + 1:
        case cpu::addr_ime + 2:
        case cpu::addr_ime + 3: return 0_u8;
        case cpu::addr_ie:      return narrow<u8>(cpu_.ie_);
        case cpu::addr_ie + 1:  return narrow<u8>(cpu_.ie_ >> 8_u8);
        case cpu::addr_if:      return narrow<u8>(cpu_.if_);
        case cpu::addr_if + 1:  return narrow<u8>(cpu_.if_ >> 8_u8);
        case cpu::addr_waitcnt:
            return cpu_.waitcnt_.sram
              | (cpu_.waitcnt_.ws0_nonseq << 2_u8)
              | (cpu_.waitcnt_.ws0_seq << 4_u8)
              | (cpu_.waitcnt_.ws1_nonseq << 5_u8)
              | (cpu_.waitcnt_.ws1_seq << 7_u8);
        case cpu::addr_waitcnt + 1:
            return cpu_.waitcnt_.ws2_nonseq
              | (cpu_.waitcnt_.ws2_seq << 2_u8)
              | (cpu_.waitcnt_.phi << 3_u8)
              | bit::from_bool<u8>(cpu_.waitcnt_.prefetch_buffer_enable) << 6_u8;
        case cpu::addr_waitcnt + 2:
        case cpu::addr_waitcnt + 3:
            return 0_u8;
        case cpu::addr_postboot:
            return cpu_.post_boot_;
    }

    return narrow<u8>(cpu_.read_unused(addr));
}

void core::write_io(const u32 addr, const u8 data) noexcept
{
    auto& timer_controller = cpu_.timer_controller_;
    auto& dma_controller = cpu_.dma_controller_;

    const auto win_enable_write = [](ppu::win_enable_bits& area, const u8 data) {
        area.bg_enabled[0_usize] = bit::test(data, 0_u8);
        area.bg_enabled[1_usize] = bit::test(data, 1_u8);
        area.bg_enabled[2_usize] = bit::test(data, 2_u8);
        area.bg_enabled[3_usize] = bit::test(data, 3_u8);
        area.obj_enabled = bit::test(data, 4_u8);
        area.blend_enabled = bit::test(data, 5_u8);
    };

    switch(addr.get()) {
        case keypad::addr_control:
            keypad_.keycnt_.select = bit::set_byte(keypad_.keycnt_.select, 0_u8, data);
            if(keypad_.interrupt_available()) {
                cpu_.request_interrupt(cpu::interrupt_source::keypad);
            }
            break;
        case keypad::addr_control + 1:
            keypad_.keycnt_.select = bit::set_byte(keypad_.keycnt_.select, 1_u8, data & 0b11_u8);
            keypad_.keycnt_.enabled = bit::test(data, 6_u8);
            keypad_.keycnt_.cond_strategy = to_enum<keypad::irq_control::condition_strategy>(bit::extract(data, 7_u8));
            if(keypad_.interrupt_available()) {
                cpu_.request_interrupt(cpu::interrupt_source::keypad);
            }
            break;

        case ppu::addr_dispcnt:
            ppu_engine_.dispcnt_.bg_mode = data & 0b111_u8;
            ppu_engine_.dispcnt_.frame_select = bit::extract(data, 4_u8);
            ppu_engine_.dispcnt_.hblank_interval_free = bit::test(data, 5_u8);
            ppu_engine_.dispcnt_.obj_mapping_1d = bit::test(data, 6_u8);
            ppu_engine_.dispcnt_.forced_blank = bit::test(data, 7_u8);
            break;
        case ppu::addr_dispcnt + 1:
            ppu_engine_.dispcnt_.obj_enabled = bit::test(data, 4_u8);
            ppu_engine_.dispcnt_.win0_enabled = bit::test(data, 5_u8);
            ppu_engine_.dispcnt_.win1_enabled = bit::test(data, 6_u8);
            ppu_engine_.dispcnt_.win_obj_enabled = bit::test(data, 7_u8);
            for(u8 bg = 0_u8; bg < ppu_engine_.dispcnt_.bg_enabled.size(); ++bg) {
                ppu_engine_.dispcnt_.bg_enabled[bg] = bit::test(data, bg);
            }
            break;
        case ppu::addr_greenswap: ppu_engine_.green_swap_ = bit::test(data, 0_u8); break;
        case ppu::addr_dispstat:
            ppu_engine_.dispstat_.vblank_irq_enabled = bit::test(data, 3_u8);
            ppu_engine_.dispstat_.hblank_irq_enabled = bit::test(data, 4_u8);
            ppu_engine_.dispstat_.vcounter_irq_enabled = bit::test(data, 5_u8);
            ppu_engine_.check_vcounter_irq();
            break;
        case ppu::addr_dispstat + 1:
            ppu_engine_.dispstat_.vcount_setting = data;
            ppu_engine_.check_vcounter_irq();
            break;
        case ppu::addr_bg0cnt:      ppu_engine_.bg0_.cnt.write_lower(data); break;
        case ppu::addr_bg0cnt + 1:  ppu_engine_.bg0_.cnt.write_upper(data); break;
        case ppu::addr_bg1cnt:      ppu_engine_.bg1_.cnt.write_lower(data); break;
        case ppu::addr_bg1cnt + 1:  ppu_engine_.bg1_.cnt.write_upper(data); break;
        case ppu::addr_bg2cnt:      ppu_engine_.bg2_.cnt.write_lower(data); break;
        case ppu::addr_bg2cnt + 1:  ppu_engine_.bg2_.cnt.write_upper(data); break;
        case ppu::addr_bg3cnt:      ppu_engine_.bg3_.cnt.write_lower(data); break;
        case ppu::addr_bg3cnt + 1:  ppu_engine_.bg3_.cnt.write_upper(data); break;
        case ppu::addr_bg0hofs:     ppu_engine_.bg0_.hoffset = bit::set_byte(ppu_engine_.bg0_.hoffset, 0_u8, data); break;
        case ppu::addr_bg0hofs + 1: ppu_engine_.bg0_.hoffset = bit::set_byte(ppu_engine_.bg0_.hoffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg0vofs:     ppu_engine_.bg0_.voffset = bit::set_byte(ppu_engine_.bg0_.voffset, 0_u8, data); break;
        case ppu::addr_bg0vofs + 1: ppu_engine_.bg0_.voffset = bit::set_byte(ppu_engine_.bg0_.voffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg1hofs:     ppu_engine_.bg1_.hoffset = bit::set_byte(ppu_engine_.bg1_.hoffset, 0_u8, data); break;
        case ppu::addr_bg1hofs + 1: ppu_engine_.bg1_.hoffset = bit::set_byte(ppu_engine_.bg1_.hoffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg1vofs:     ppu_engine_.bg1_.voffset = bit::set_byte(ppu_engine_.bg1_.voffset, 0_u8, data); break;
        case ppu::addr_bg1vofs + 1: ppu_engine_.bg1_.voffset = bit::set_byte(ppu_engine_.bg1_.voffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg2hofs:     ppu_engine_.bg2_.hoffset = bit::set_byte(ppu_engine_.bg2_.hoffset, 0_u8, data); break;
        case ppu::addr_bg2hofs + 1: ppu_engine_.bg2_.hoffset = bit::set_byte(ppu_engine_.bg2_.hoffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg2vofs:     ppu_engine_.bg2_.voffset = bit::set_byte(ppu_engine_.bg2_.voffset, 0_u8, data); break;
        case ppu::addr_bg2vofs + 1: ppu_engine_.bg2_.voffset = bit::set_byte(ppu_engine_.bg2_.voffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg3hofs:     ppu_engine_.bg3_.hoffset = bit::set_byte(ppu_engine_.bg3_.hoffset, 0_u8, data); break;
        case ppu::addr_bg3hofs + 1: ppu_engine_.bg3_.hoffset = bit::set_byte(ppu_engine_.bg3_.hoffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg3vofs:     ppu_engine_.bg3_.voffset = bit::set_byte(ppu_engine_.bg3_.voffset, 0_u8, data); break;
        case ppu::addr_bg3vofs + 1: ppu_engine_.bg3_.voffset = bit::set_byte(ppu_engine_.bg3_.voffset, 1_u8, bit::extract(data, 0_u8)); break;
        case ppu::addr_bg2pa:       ppu_engine_.bg2_.pa = bit::set_byte(ppu_engine_.bg2_.pa, 0_u8, data); break;
        case ppu::addr_bg2pa + 1:   ppu_engine_.bg2_.pa = bit::set_byte(ppu_engine_.bg2_.pa, 1_u8, data); break;
        case ppu::addr_bg2pb:       ppu_engine_.bg2_.pb = bit::set_byte(ppu_engine_.bg2_.pb, 0_u8, data); break;
        case ppu::addr_bg2pb + 1:   ppu_engine_.bg2_.pb = bit::set_byte(ppu_engine_.bg2_.pb, 1_u8, data); break;
        case ppu::addr_bg2pc:       ppu_engine_.bg2_.pc = bit::set_byte(ppu_engine_.bg2_.pc, 0_u8, data); break;
        case ppu::addr_bg2pc + 1:   ppu_engine_.bg2_.pc = bit::set_byte(ppu_engine_.bg2_.pc, 1_u8, data); break;
        case ppu::addr_bg2pd:       ppu_engine_.bg2_.pd = bit::set_byte(ppu_engine_.bg2_.pd, 0_u8, data); break;
        case ppu::addr_bg2pd + 1:   ppu_engine_.bg2_.pd = bit::set_byte(ppu_engine_.bg2_.pd, 1_u8, data); break;
        case ppu::addr_bg2x:        ppu_engine_.bg2_.x_ref.set_byte<0_u8>(data); break;
        case ppu::addr_bg2x + 1:    ppu_engine_.bg2_.x_ref.set_byte<1_u8>(data); break;
        case ppu::addr_bg2x + 2:    ppu_engine_.bg2_.x_ref.set_byte<2_u8>(data); break;
        case ppu::addr_bg2x + 3:    ppu_engine_.bg2_.x_ref.set_byte<3_u8>(data); break;
        case ppu::addr_bg2y:        ppu_engine_.bg2_.y_ref.set_byte<0_u8>(data); break;
        case ppu::addr_bg2y + 1:    ppu_engine_.bg2_.y_ref.set_byte<1_u8>(data); break;
        case ppu::addr_bg2y + 2:    ppu_engine_.bg2_.y_ref.set_byte<2_u8>(data); break;
        case ppu::addr_bg2y + 3:    ppu_engine_.bg2_.y_ref.set_byte<3_u8>(data); break;
        case ppu::addr_bg3pa:       ppu_engine_.bg3_.pa = bit::set_byte(ppu_engine_.bg3_.pa, 0_u8, data); break;
        case ppu::addr_bg3pa + 1:   ppu_engine_.bg3_.pa = bit::set_byte(ppu_engine_.bg3_.pa, 1_u8, data); break;
        case ppu::addr_bg3pb:       ppu_engine_.bg3_.pb = bit::set_byte(ppu_engine_.bg3_.pb, 0_u8, data); break;
        case ppu::addr_bg3pb + 1:   ppu_engine_.bg3_.pb = bit::set_byte(ppu_engine_.bg3_.pb, 1_u8, data); break;
        case ppu::addr_bg3pc:       ppu_engine_.bg3_.pc = bit::set_byte(ppu_engine_.bg3_.pc, 0_u8, data); break;
        case ppu::addr_bg3pc + 1:   ppu_engine_.bg3_.pc = bit::set_byte(ppu_engine_.bg3_.pc, 1_u8, data); break;
        case ppu::addr_bg3pd:       ppu_engine_.bg3_.pd = bit::set_byte(ppu_engine_.bg3_.pd, 0_u8, data); break;
        case ppu::addr_bg3pd + 1:   ppu_engine_.bg3_.pd = bit::set_byte(ppu_engine_.bg3_.pd, 1_u8, data); break;
        case ppu::addr_bg3x:        ppu_engine_.bg3_.x_ref.set_byte<0_u8>(data); break;
        case ppu::addr_bg3x + 1:    ppu_engine_.bg3_.x_ref.set_byte<1_u8>(data); break;
        case ppu::addr_bg3x + 2:    ppu_engine_.bg3_.x_ref.set_byte<2_u8>(data); break;
        case ppu::addr_bg3x + 3:    ppu_engine_.bg3_.x_ref.set_byte<3_u8>(data); break;
        case ppu::addr_bg3y:        ppu_engine_.bg3_.y_ref.set_byte<0_u8>(data); break;
        case ppu::addr_bg3y + 1:    ppu_engine_.bg3_.y_ref.set_byte<1_u8>(data); break;
        case ppu::addr_bg3y + 2:    ppu_engine_.bg3_.y_ref.set_byte<2_u8>(data); break;
        case ppu::addr_bg3y + 3:    ppu_engine_.bg3_.y_ref.set_byte<3_u8>(data); break;

        case ppu::addr_win0h:       ppu_engine_.win0_.bottom_right.x = data; break;
        case ppu::addr_win0h + 1:   ppu_engine_.win0_.top_left.x = data; break;
        case ppu::addr_win1h:       ppu_engine_.win1_.bottom_right.x = data; break;
        case ppu::addr_win1h + 1:   ppu_engine_.win1_.top_left.x = data; break;
        case ppu::addr_win0v:       ppu_engine_.win0_.bottom_right.y = data; break;
        case ppu::addr_win0v + 1:   ppu_engine_.win0_.top_left.y = data; break;
        case ppu::addr_win1v:       ppu_engine_.win1_.bottom_right.y = data; break;
        case ppu::addr_win1v + 1:   ppu_engine_.win1_.top_left.y = data; break;
        case ppu::addr_winin:       win_enable_write(ppu_engine_.win_in_.win0, data); break;
        case ppu::addr_winin + 1:   win_enable_write(ppu_engine_.win_in_.win1, data); break;
        case ppu::addr_winout:      win_enable_write(ppu_engine_.win_out_.outside, data); break;
        case ppu::addr_winout + 1:  win_enable_write(ppu_engine_.win_out_.obj, data); break;
        case ppu::addr_mosaic:
            ppu_engine_.mosaic_bg_.h = (data & 0xF_u8) + 1_u8;
            ppu_engine_.mosaic_bg_.v = (data >> 4_u8) + 1_u8;
            ppu_engine_.mosaic_bg_.internal.v = 0_u8;
            break;
        case ppu::addr_mosaic + 1:
            ppu_engine_.mosaic_obj_.h = (data & 0xF_u8) + 1_u8;
            ppu_engine_.mosaic_obj_.v = (data >> 4_u8) + 1_u8;
            ppu_engine_.mosaic_obj_.internal.v = 0_u8;
            break;
        case ppu::addr_bldcnt:
            ppu_engine_.bldcnt_.first.bg[0_usize] = bit::test(data, 0_u8);
            ppu_engine_.bldcnt_.first.bg[1_usize] = bit::test(data, 1_u8);
            ppu_engine_.bldcnt_.first.bg[2_usize] = bit::test(data, 2_u8);
            ppu_engine_.bldcnt_.first.bg[3_usize] = bit::test(data, 3_u8);
            ppu_engine_.bldcnt_.first.obj = bit::test(data, 4_u8);
            ppu_engine_.bldcnt_.first.backdrop = bit::test(data, 5_u8);
            ppu_engine_.bldcnt_.type = to_enum<ppu::bldcnt::effect>(data >> 6_u8);
            break;
        case ppu::addr_bldcnt + 1:
            ppu_engine_.bldcnt_.second.bg[0_usize] = bit::test(data, 0_u8);
            ppu_engine_.bldcnt_.second.bg[1_usize] = bit::test(data, 1_u8);
            ppu_engine_.bldcnt_.second.bg[2_usize] = bit::test(data, 2_u8);
            ppu_engine_.bldcnt_.second.bg[3_usize] = bit::test(data, 3_u8);
            ppu_engine_.bldcnt_.second.obj = bit::test(data, 4_u8);
            ppu_engine_.bldcnt_.second.backdrop = bit::test(data, 5_u8);
            break;
        case ppu::addr_bldalpha:     ppu_engine_.blend_settings_.eva = data & 0x1F_u8; break;
        case ppu::addr_bldalpha + 1: ppu_engine_.blend_settings_.evb = data & 0x1F_u8; break;
        case ppu::addr_bldy:         ppu_engine_.blend_settings_.evy = data & 0x1F_u8; break;

        case apu::addr_sound1cnt_l:     apu_engine_.write<1>(apu::pulse_channel::register_index::sweep, data); break;
        case apu::addr_sound1cnt_h:     apu_engine_.write<1>(apu::pulse_channel::register_index::wave_data, data); break;
        case apu::addr_sound1cnt_h + 1: apu_engine_.write<1>(apu::pulse_channel::register_index::envelope, data); break;
        case apu::addr_sound1cnt_x:     apu_engine_.write<1>(apu::pulse_channel::register_index::freq_data, data); break;
        case apu::addr_sound1cnt_x + 1: apu_engine_.write<1>(apu::pulse_channel::register_index::freq_control, data); break;
        case apu::addr_sound2cnt_l:     apu_engine_.write<2>(apu::pulse_channel::register_index::wave_data, data); break;
        case apu::addr_sound2cnt_l + 1: apu_engine_.write<2>(apu::pulse_channel::register_index::envelope, data); break;
        case apu::addr_sound2cnt_h:     apu_engine_.write<2>(apu::pulse_channel::register_index::freq_data, data); break;
        case apu::addr_sound2cnt_h + 1: apu_engine_.write<2>(apu::pulse_channel::register_index::freq_control, data); break;
        case apu::addr_sound3cnt_l:     apu_engine_.write<3>(apu::wave_channel::register_index::enable, data); break;
        case apu::addr_sound3cnt_h:     apu_engine_.write<3>(apu::wave_channel::register_index::sound_length, data); break;
        case apu::addr_sound3cnt_h + 1: apu_engine_.write<3>(apu::wave_channel::register_index::output_level, data); break;
        case apu::addr_sound3cnt_x:     apu_engine_.write<3>(apu::wave_channel::register_index::freq_data, data); break;
        case apu::addr_sound3cnt_x + 1: apu_engine_.write<3>(apu::wave_channel::register_index::freq_control, data); break;
        case apu::addr_sound4cnt_l:     apu_engine_.write<4>(apu::noise_channel::register_index::sound_length, data); break;
        case apu::addr_sound4cnt_l + 1: apu_engine_.write<4>(apu::noise_channel::register_index::envelope, data); break;
        case apu::addr_sound4cnt_h:     apu_engine_.write<4>(apu::noise_channel::register_index::polynomial_counter, data); break;
        case apu::addr_sound4cnt_h + 1: apu_engine_.write<4>(apu::noise_channel::register_index::freq_control, data); break;
        case apu::addr_soundcnt_l:      apu_engine_.control_.write<0>(data); break;
        case apu::addr_soundcnt_l + 1:  apu_engine_.control_.write<1>(data); break;
        case apu::addr_soundcnt_h:      apu_engine_.control_.write<2>(data); break;
        case apu::addr_soundcnt_h + 1: {
            apu_engine_.control_.write<3>(data);
            if(bit::test(data, 3_u8)) { apu_engine_.fifo_a_.reset(); }
            if(bit::test(data, 7_u8)) { apu_engine_.fifo_b_.reset(); }
            break;
        }
        case apu::addr_soundcnt_x:
            if(!bit::test(data, 7_u8)) {
                for(u32 apu_reg_addr : range(apu::addr_sound1cnt_l, apu::addr_soundcnt_l)) {
                    write_io(apu_reg_addr, 0x00_u8);
                }

                apu_engine_.channel_1_.disable();
                apu_engine_.channel_2_.disable();
                apu_engine_.channel_3_.disable();
                apu_engine_.channel_4_.disable();

                apu_engine_.power_on_ = false;
            } else if(!apu_engine_.power_on_) {
                apu_engine_.frame_sequencer_ = 0_u8;
                apu_engine_.power_on_ = true;
            }
            break;
        case apu::addr_soundbias:
            apu_engine_.soundbias_.bias = bit::set_byte(apu_engine_.soundbias_.bias, 0_u8, bit::clear(data, 0_u8));
            break;
        case apu::addr_soundbias + 1:
            apu_engine_.soundbias_.bias = bit::set_byte(apu_engine_.soundbias_.bias, 1_u8, data & 0b11_u8);
            apu_engine_.soundbias_.resolution = data >> 6_u8;
            apu_engine_.resampler_.set_src_sample_rate(apu_engine_.soundbias_.sample_rate());
            break;
        case apu::addr_wave_ram:      case apu::addr_wave_ram + 1:  case apu::addr_wave_ram + 2:  case apu::addr_wave_ram + 3:
        case apu::addr_wave_ram + 4:  case apu::addr_wave_ram + 5:  case apu::addr_wave_ram + 6:  case apu::addr_wave_ram + 7:
        case apu::addr_wave_ram + 8:  case apu::addr_wave_ram + 9:  case apu::addr_wave_ram + 10: case apu::addr_wave_ram + 11:
        case apu::addr_wave_ram + 12: case apu::addr_wave_ram + 13: case apu::addr_wave_ram + 14: case apu::addr_wave_ram + 15:
            apu_engine_.channel_3_.write_wave_ram(addr & 0xF_u32, data);
            break;
        case apu::addr_fifo_a: case apu::addr_fifo_a + 1: case apu::addr_fifo_a + 2: case apu::addr_fifo_a + 3:
            apu_engine_.fifo_a_.write(data);
            break;
        case apu::addr_fifo_b: case apu::addr_fifo_b + 1: case apu::addr_fifo_b + 2: case apu::addr_fifo_b + 3:
            apu_engine_.fifo_b_.write(data);
            break;

        // cnt_h_msb is unused
        case cpu::addr_tm0cnt_l:     timer_controller[0_usize].write(timer::register_type::cnt_l_lsb, data); break;
        case cpu::addr_tm0cnt_l + 1: timer_controller[0_usize].write(timer::register_type::cnt_l_msb, data); break;
        case cpu::addr_tm0cnt_h:     timer_controller[0_usize].write(timer::register_type::cnt_h_lsb, data); break;
        case cpu::addr_tm1cnt_l:     timer_controller[1_usize].write(timer::register_type::cnt_l_lsb, data); break;
        case cpu::addr_tm1cnt_l + 1: timer_controller[1_usize].write(timer::register_type::cnt_l_msb, data); break;
        case cpu::addr_tm1cnt_h:     timer_controller[1_usize].write(timer::register_type::cnt_h_lsb, data); break;
        case cpu::addr_tm2cnt_l:     timer_controller[2_usize].write(timer::register_type::cnt_l_lsb, data); break;
        case cpu::addr_tm2cnt_l + 1: timer_controller[2_usize].write(timer::register_type::cnt_l_msb, data); break;
        case cpu::addr_tm2cnt_h:     timer_controller[2_usize].write(timer::register_type::cnt_h_lsb, data); break;
        case cpu::addr_tm3cnt_l:     timer_controller[3_usize].write(timer::register_type::cnt_l_lsb, data); break;
        case cpu::addr_tm3cnt_l + 1: timer_controller[3_usize].write(timer::register_type::cnt_l_msb, data); break;
        case cpu::addr_tm3cnt_h:     timer_controller[3_usize].write(timer::register_type::cnt_h_lsb, data); break;

        case cpu::addr_dma0sad:       dma_controller[0_usize].write_src(0_u8, data); break;
        case cpu::addr_dma0sad + 1:   dma_controller[0_usize].write_src(1_u8, data); break;
        case cpu::addr_dma0sad + 2:   dma_controller[0_usize].write_src(2_u8, data); break;
        case cpu::addr_dma0sad + 3:   dma_controller[0_usize].write_src(3_u8, data); break;
        case cpu::addr_dma0dad:       dma_controller[0_usize].write_dst(0_u8, data); break;
        case cpu::addr_dma0dad + 1:   dma_controller[0_usize].write_dst(1_u8, data); break;
        case cpu::addr_dma0dad + 2:   dma_controller[0_usize].write_dst(2_u8, data); break;
        case cpu::addr_dma0dad + 3:   dma_controller[0_usize].write_dst(3_u8, data); break;
        case cpu::addr_dma0cnt_l:     dma_controller[0_usize].write_count(0_u8, data); break;
        case cpu::addr_dma0cnt_l + 1: dma_controller[0_usize].write_count(1_u8, data); break;
        case cpu::addr_dma0cnt_h:     dma_controller.write_cnt_l(0_usize, data); break;
        case cpu::addr_dma0cnt_h + 1: dma_controller.write_cnt_h(0_usize, data); break;
        case cpu::addr_dma1sad:       dma_controller[1_usize].write_src(0_u8, data); break;
        case cpu::addr_dma1sad + 1:   dma_controller[1_usize].write_src(1_u8, data); break;
        case cpu::addr_dma1sad + 2:   dma_controller[1_usize].write_src(2_u8, data); break;
        case cpu::addr_dma1sad + 3:   dma_controller[1_usize].write_src(3_u8, data); break;
        case cpu::addr_dma1dad:       dma_controller[1_usize].write_dst(0_u8, data); break;
        case cpu::addr_dma1dad + 1:   dma_controller[1_usize].write_dst(1_u8, data); break;
        case cpu::addr_dma1dad + 2:   dma_controller[1_usize].write_dst(2_u8, data); break;
        case cpu::addr_dma1dad + 3:   dma_controller[1_usize].write_dst(3_u8, data); break;
        case cpu::addr_dma1cnt_l:     dma_controller[1_usize].write_count(0_u8, data); break;
        case cpu::addr_dma1cnt_l + 1: dma_controller[1_usize].write_count(1_u8, data); break;
        case cpu::addr_dma1cnt_h:     dma_controller.write_cnt_l(1_usize, data); break;
        case cpu::addr_dma1cnt_h + 1: dma_controller.write_cnt_h(1_usize, data); break;
        case cpu::addr_dma2sad:       dma_controller[2_usize].write_src(0_u8, data); break;
        case cpu::addr_dma2sad + 1:   dma_controller[2_usize].write_src(1_u8, data); break;
        case cpu::addr_dma2sad + 2:   dma_controller[2_usize].write_src(2_u8, data); break;
        case cpu::addr_dma2sad + 3:   dma_controller[2_usize].write_src(3_u8, data); break;
        case cpu::addr_dma2dad:       dma_controller[2_usize].write_dst(0_u8, data); break;
        case cpu::addr_dma2dad + 1:   dma_controller[2_usize].write_dst(1_u8, data); break;
        case cpu::addr_dma2dad + 2:   dma_controller[2_usize].write_dst(2_u8, data); break;
        case cpu::addr_dma2dad + 3:   dma_controller[2_usize].write_dst(3_u8, data); break;
        case cpu::addr_dma2cnt_l:     dma_controller[2_usize].write_count(0_u8, data); break;
        case cpu::addr_dma2cnt_l + 1: dma_controller[2_usize].write_count(1_u8, data); break;
        case cpu::addr_dma2cnt_h:     dma_controller.write_cnt_l(2_usize, data); break;
        case cpu::addr_dma2cnt_h + 1: dma_controller.write_cnt_h(2_usize, data); break;
        case cpu::addr_dma3sad:       dma_controller[3_usize].write_src(0_u8, data); break;
        case cpu::addr_dma3sad + 1:   dma_controller[3_usize].write_src(1_u8, data); break;
        case cpu::addr_dma3sad + 2:   dma_controller[3_usize].write_src(2_u8, data); break;
        case cpu::addr_dma3sad + 3:   dma_controller[3_usize].write_src(3_u8, data); break;
        case cpu::addr_dma3dad:       dma_controller[3_usize].write_dst(0_u8, data); break;
        case cpu::addr_dma3dad + 1:   dma_controller[3_usize].write_dst(1_u8, data); break;
        case cpu::addr_dma3dad + 2:   dma_controller[3_usize].write_dst(2_u8, data); break;
        case cpu::addr_dma3dad + 3:   dma_controller[3_usize].write_dst(3_u8, data); break;
        case cpu::addr_dma3cnt_l:     dma_controller[3_usize].write_count(0_u8, data); break;
        case cpu::addr_dma3cnt_l + 1: dma_controller[3_usize].write_count(1_u8, data); break;
        case cpu::addr_dma3cnt_h:     dma_controller.write_cnt_l(3_usize, data); break;
        case cpu::addr_dma3cnt_h + 1: dma_controller.write_cnt_h(3_usize, data); break;

        case cpu::addr_ime:
            cpu_.ime_ = bit::test(data, 0_u8);
            cpu_.schedule_update_irq_signal();
            break;
        case cpu::addr_ie:
            cpu_.ie_ = bit::set_byte(cpu_.ie_, 0_u8, data);
            cpu_.schedule_update_irq_signal();
            break;
        case cpu::addr_ie + 1:
            cpu_.ie_ = bit::set_byte(cpu_.ie_, 1_u8, data & 0x3F_u8);
            cpu_.schedule_update_irq_signal();
            break;
        case cpu::addr_if:
            cpu_.if_ &= ~data;
            cpu_.schedule_update_irq_signal();
            break;
        case cpu::addr_if + 1:
            cpu_.if_ &= ~(widen<u16>(data) << 8_u16);
            cpu_.schedule_update_irq_signal();
            break;
        case cpu::addr_waitcnt:
            cpu_.waitcnt_.sram = data & 0b11_u8;
            cpu_.waitcnt_.ws0_nonseq = (data >> 2_u8) & 0b11_u8;
            cpu_.waitcnt_.ws0_seq = bit::extract(data, 4_u8);
            cpu_.waitcnt_.ws1_nonseq = (data >> 5_u8) & 0b11_u8;
            cpu_.waitcnt_.ws1_seq = bit::extract(data, 7_u8);
            cpu_.update_waitstate_table();
            break;
        case cpu::addr_waitcnt + 1:
            cpu_.waitcnt_.ws2_nonseq = data & 0b11_u8;
            cpu_.waitcnt_.ws2_seq = bit::extract(data, 2_u8);
            cpu_.waitcnt_.phi = (data >> 3_u8) & 0b11_u8;
            cpu_.waitcnt_.prefetch_buffer_enable = bit::test(data, 6_u8);
            cpu_.update_waitstate_table();
            break;
        case cpu::addr_haltcnt:
            cpu_.haltcnt_ = to_enum<cpu::halt_control>(bit::extract(data, 7_u8));
            break;
        case cpu::addr_postboot:
            cpu_.post_boot_ = bit::extract(data, 0_u8);
            break;
    }
}

} // namespace gba
