#ifndef GAMEBOIADVANCE_DISASSEMBLER_H
#define GAMEBOIADVANCE_DISASSEMBLER_H

#include <string>

#include <gba/helper/function_ptr.h>
#include <gba/helper/lookup_table.h>

namespace gba::debugger {

class disassembler {
    lookup_table<function_ptr<std::string(u32, u32)>, 12_u32> arm_table_;
    lookup_table<function_ptr<std::string(u32, u16)>, 10_u32> thumb_table_;

public:
    disassembler() noexcept;

    [[nodiscard]] std::string disassemble_arm(u32 address, u32 instruction) const noexcept;
    [[nodiscard]] std::string disassemble_thumb(u32 address, u16 instruction) const noexcept;
};

} // namespace gba::debugger

#endif //GAMEBOIADVANCE_DISASSEMBLER_H
