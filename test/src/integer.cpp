/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <test_prelude.h>

using namespace gba;

// test cases in comments should produce compile errors, which is correct behaviour

TEST_CASE("integer")
{
    SUBCASE("default value") {
        i32 i;
        CHECK(i == 0_i32);
    }

    SUBCASE("construct & assign") {
        // integer<bool> b = true;                          // invalid integer type
        // integer<std::string> s = std::string("str");     // invalid integer type

        i32 i{4_i32};
        CHECK(i == 4_i32);
        i32 i2 = i;
        CHECK(i2 == 4_i32);

        u32 u1{12_u32};
        CHECK(u1 == 12_u32);

        i32 i3 = narrow<u16>(u1);   // lower width unsigned to signed is allowed
        CHECK(i3 == 12_i32);
        // i32 i3 = u1;             // same or more width unsigned to signed might overflow
        // u32 u = i2;              // signed to unsigned not allowed
    }

    SUBCASE("arithmetic") {
        i32 i{1_i32};
        u32 u{1_u32};

        CHECK(i == +i);
        CHECK(u == +u);
        CHECK(-i == -1_i32);

        CHECK(2_i32 == ++i);
        CHECK(2_u32 == ++u);

        CHECK(1_i32 == --i);
        CHECK(1_u32 == --u);

        // others work if these work, too
        i32 i2 = 3_i32;
        i2 += i;
        CHECK(i2 == 4_i32);

        i32 i3 = 3_i32;
        i3 -= i;
        CHECK(i3 == 2_i32);
    }

    SUBCASE("bitwise") {
        // ~i16{0x000F_i16};        // integer bitwise ops not allowed

        u16 u = 0xFFF0_u16;
        CHECK(~u == 0x000F_u16);

        // non compound assignement ones should work too, if these work
        u |= 0x000F_u16;
        CHECK(u == 0xFFFF_u16);

        u &= 0x000F_u16;
        CHECK(u == 0x000F_u16);

        u ^= 0x00FF_u16;
        CHECK(u == 0x00F0_u16);

        u <<= 4_u8;
        CHECK(u == 0x0F00_u16);

        u >>= 8_u8;
        CHECK(u == 0x000F_u16);

        i16 i = 0xF000_i16;
        i >>= 8_i8;
        CHECK(i == 0xFFF0_i16);     // shifting also allowed in signed (uses arithmetic shift)

        // u |= 0x0000'0000_u32;    // larger width unsigned is not allowed on right-hand side
    }

    SUBCASE("sign conversion & narrow") {
        // using auto to hide the type

        u32 u1{1_u32};
        auto i1 = make_signed(u1);
        static_assert(std::is_same_v<decltype(i1), i32>, "i1 should've been i32");
        CHECK(i1 == 1_i32);

        i32 i2{1_i32};
        auto u2 = make_unsigned(i2);
        static_assert(std::is_same_v<decltype(u2), u32>, "u2 should've been u32");
        CHECK(u2 == 1_u32);

        u32 u3 = 0xFEDC'3210_u32;
        auto u4 = narrow<u16>(u3);
        static_assert(std::is_same_v<decltype(u4), u16>, "u4 should've been u16");
        CHECK(u4 == 0x3210_u16);

        auto u5 = narrow<u32>(u3);
        static_assert(std::is_same_v<decltype(u5), u32>, "u5 should've been u32");
        CHECK(u5 == 0xFEDC'3210_u32);

        // narrow doesn't widen the integers
        // auto u = narrow<u64>(u3);
    }

    SUBCASE("comparison") {
        // either side should be safely convertible to the other

        i32 signed_int = 0;
        u32 unsigned_int = 0_u32;

        CHECK(signed_int == 0_i8);
        CHECK(signed_int == 0_i16);
        CHECK(signed_int == 0_i32);
        CHECK(signed_int == 0_i64); // wider type also works

        CHECK(signed_int == 0_u8);
        CHECK(signed_int == 0_u16);
        // CHECK(signed_int == 0_u32);
        // CHECK(signed_int == 0_u64);

        // CHECK(unsigned_int == 0_i8);
        // CHECK(unsigned_int == 0_i16);
        // CHECK(unsigned_int == 0_i32);
        // CHECK(unsigned_int == 0_i64);

        CHECK(unsigned_int == 0_u8);
        CHECK(unsigned_int == 0_u16);
        CHECK(unsigned_int == 0_u32);
        CHECK(unsigned_int == 0_u64); // wider type also works
    }
}
