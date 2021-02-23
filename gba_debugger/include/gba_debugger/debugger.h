/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_DEBUGGER_H
#define GAMEBOIADVANCE_DEBUGGER_H

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

#include <gba/core/fwd.h>
#include <gba_debugger/memory_debugger.h>
#include <gba_debugger/gamepak_debugger.h>
#include <gba_debugger/arm_debugger.h>
#include <gba_debugger/ppu_debugger.h>
#include <gba_debugger/keypad_debugger.h>

namespace gba::debugger {

class window {
    sf::RenderWindow window_;
    sf::Event window_event_;
    sf::Clock dt_;

    core* core_;
    disassembly_view disassembly_view_;
    memory_view memory_view_;
    gamepak_debugger gamepak_debugger_;
    arm_debugger arm_debugger_;
    ppu_debugger ppu_debugger_;
    keypad_debugger keypad_debugger_;

public:
    explicit window(core* core) noexcept;

    [[nodiscard]] bool draw() noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_DEBUGGER_H
