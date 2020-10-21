#ifndef GAMEBOIADVANCE_LOOKUP_TABLE_H
#define GAMEBOIADVANCE_LOOKUP_TABLE_H

#include <gba/core/container.h>

namespace gba {

template<typename T, u32::type BitCount>
class lookup_table {
    array<T, 1_u32 << BitCount> table_;

public:
    struct init_data {
        std::string_view expr;
        T data;
    };

    constexpr lookup_table(std::initializer_list<init_data> table_init_data) noexcept
    {
        for(const auto& data : table_init_data) {
            init(data);
        }
    }

    constexpr T operator[](usize index) const noexcept { return table_[index]; }

private:
    constexpr void init(const init_data& data) noexcept
    {
        u32 xmask;
        u32 kmask;
        for(u32 i = 0u; i < BitCount; ++i) {
            auto idx = BitCount - 1u - i;
            auto c = data.expr[idx.get()];
            switch(c) {
                case 'x': xmask |= (1u << i); break;
                case '1': kmask |= (1u << i); break;
                case '0': break; // nop
                default: UNREACHABLE();
            }
        }

        init_recursive(kmask, xmask, data.data);
    }

    constexpr void init_recursive(const u32 kmask, const u32 xmask, const T& value) noexcept
    {
        if(xmask == 0u) {
            table_[kmask] = value;
        } else {
            const u32 xmask_lsb = ~(xmask - 1u) & xmask;
            const u32 xmask_without_lsb = xmask & ~xmask_lsb;
            init_recursive(kmask, xmask_without_lsb, value);
            init_recursive(kmask | xmask_lsb, xmask_without_lsb, value);
        }
    }
};

} // namespace gba

#endif //GAMEBOIADVANCE_LOOKUP_TABLE_H
