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

#include <gba_debugger/gamepak_debugger.h>
#include <gba_debugger/arm_debugger.h>

namespace gba {

struct gba;

namespace debugger {

class window {
    sf::RenderWindow window_;
    sf::Event window_event_;
    sf::Clock dt_;

    gamepak_debugger gamepak_debugger_;
    arm_debugger arm_debugger_;

public:
    explicit window(gba* g) noexcept;

    [[nodiscard]] bool draw() noexcept;
};

} // namespace debugger
} // namespace gba

#endif //GAMEBOIADVANCE_DEBUGGER_H
