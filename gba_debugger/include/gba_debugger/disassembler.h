#ifndef GAMEBOIADVANCE_DISASSEMBLER_H
#define GAMEBOIADVANCE_DISASSEMBLER_H

#include <string>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

#include <gba/core/integer.h>
#include <gba/core/event/delegate.h>
#include <gba/helper/lookup_table.h>

namespace gba::debugger {

class disassembler {
    lookup_table<delegate<std::string(u32, u32)>, 12_u32> arm_table_;
    lookup_table<delegate<std::string(u32)>, 4_u32> thumb_table_;

    sf::Clock dt;
    sf::RenderWindow window_;

public:
    disassembler() noexcept;

    void update(const vector<u8>& data);
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_DISASSEMBLER_H
