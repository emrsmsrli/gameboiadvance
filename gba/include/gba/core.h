/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_CORE_H
#define GAMEBOIADVANCE_CORE_H

#include <gba/core/scheduler.h>
#include <gba/arm/arm7tdmi.h>
#include <gba/cartridge/gamepak.h>
#include <gba/ppu/ppu.h>
#include <gba/keypad.h>

namespace gba {

struct core {
    scheduler schdlr;
    cartridge::gamepak pak;
    arm::arm7tdmi arm;
    ppu::engine ppu;
    keypad::keypad keypad;

    core() : core(vector<u8>{}) { LOG_ERROR(core, "no BIOS file provided"); }
    core(vector<u8> bios)
      : pak{},
        arm(this, std::move(bios)),
        ppu(&schdlr)
    {
        // ppu.set_dma_controller_handle(arm.get_dma_cnt_handle());
        // ppu.set_irq_controller_handle(arm.get_interrupt_handle());
        pak.set_irq_controller_handle(arm.get_interrupt_handle());
        //apu.set_irq_controller_handle(arm.get_interrupt_handle());
        //sio.set_irq_controller_handle(arm.get_interrupt_handle());
        //timers.set_irq_controller_handle(arm.get_interrupt_handle());

        // todo make timer controller, set prevs(cascade) here and irq controller here
    }

    void tick(u64 cycles = 1_u8) noexcept
    {
        const u64 until = schdlr.now() + cycles;
        while (schdlr.now() < until) {
            arm.tick();
        }
    }

    void tick_one_frame() noexcept { tick(ppu::engine::cycles_per_frame); }

    void release_key(const keypad::keypad::key key) noexcept { keypad.release(key); }
    void press_key(const keypad::keypad::key key) noexcept
    {
        keypad.press(key);
        if(keypad.interrupt_available()) {
            arm.request_interrupt(arm::interrupt_source::keypad);
        }
    }

    void load_pak(const fs::path& path) { pak.load(path); }

private:
    // todo = now - start - cycles_per_frame
    u64 frame_cycle_overshoot;
};

} // namespace gba

#endif //GAMEBOIADVANCE_CORE_H
