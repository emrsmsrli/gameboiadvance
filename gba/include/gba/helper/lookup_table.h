#ifndef GAMEBOIADVANCE_LOOKUP_TABLE_H
#define GAMEBOIADVANCE_LOOKUP_TABLE_H

#include <string_view>
#include <initializer_list>

#include <gba/core/container.h>

namespace gba {

template<typename T, u32::type BitCount, u32::type MaxEntries>
class lookup_table {
    array<u8, 1_u32 << BitCount> idx_table_;
    array<T, MaxEntries + 1_u32> table_; // one extra for nullptr

public:
    struct init_data {
        std::string_view expr;
        T entry;
    };

    constexpr lookup_table(std::initializer_list<init_data> table_init_data) noexcept
    {
        // init to nullptr entry
        for(u8& idx : idx_table_) { idx = narrow<u8>(u32(MaxEntries)); }

        u8 current_entry_idx = 0_u8;
        for(const auto& data : table_init_data) {
            init(data, current_entry_idx);
            ++current_entry_idx;
        }
    }

    constexpr T operator[](const usize index) const noexcept { return table_[idx_table_[index]]; }

private:
    constexpr void init(const init_data& data, const u8 entry_idx) noexcept
    {
        u32 xmask;
        u32 kmask;
        for(u32 i = 0u; i < BitCount; ++i) {
            const u32 idx = BitCount - 1_u32 - i;
            switch(data.expr[idx.get()]) {
                case 'x': xmask |= (1_u32 << i); break;
                case '1': kmask |= (1_u32 << i); break;
                case '0': break; // nop
                default: UNREACHABLE();
            }
        }

        init_recursive(kmask, xmask, data.entry, entry_idx);
    }

    constexpr void init_recursive(const u32 kmask, const u32 xmask, const T& entry, const u8 entry_idx) noexcept
    {
        if(xmask == 0_u32) {
            idx_table_[kmask] = entry_idx;
            table_[entry_idx] = entry;
        } else {
            const u32 xmask_lsb = ~(xmask - 1_u32) & xmask;
            const u32 xmask_without_lsb = xmask & ~xmask_lsb;
            init_recursive(kmask, xmask_without_lsb, entry, entry_idx);
            init_recursive(kmask | xmask_lsb, xmask_without_lsb, entry, entry_idx);
        }
    }
};

} // namespace gba

#endif //GAMEBOIADVANCE_LOOKUP_TABLE_H
