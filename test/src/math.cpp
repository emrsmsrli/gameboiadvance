/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <test_prelude.h>

#include <gba/core/math.h>

using namespace gba;

TEST_CASE("bit ops")
{
    u32 integer = 0xFEDC'3210_u32;

    SUBCASE("from_bool") {
        CHECK(bit::from_bool(true) == 1_u32);
        CHECK(bit::from_bool(false) == 0_u32);
        CHECK(bit::from_bool<u8>(false) == 0_u32);
        CHECK(bit::from_bool<u16>(false) == 0_u32);
    }

    SUBCASE("bit") {
        CHECK(bit::bit(0_u8) == 0b1_u32);
        CHECK(bit::bit(1_u8) == 0b10_u32);
        CHECK(bit::bit(2_u8) == 0b100_u32);
        CHECK(bit::bit(4_u8) == 0b10000_u32);
    }

    SUBCASE("extract") {
        CHECK(bit::extract(integer, 0_u8) == 0_u32);
        CHECK(bit::extract(integer, 1_u8) == 0_u32);
        CHECK(bit::extract(integer, 4_u8) == 1_u32);
        CHECK(bit::extract(integer, 30_u8) == 1_u32);
        CHECK(bit::extract(integer, 31_u8) == 1_u32);
    }

    SUBCASE("test") {
          CHECK_FALSE(bit::test(integer, 0_u8));
          CHECK_FALSE(bit::test(integer, 1_u8));
          CHECK(bit::test(integer, 4_u8));
          CHECK(bit::test(integer, 30_u8));
          CHECK(bit::test(integer, 31_u8));
    }

    SUBCASE("set") {
        CHECK(bit::set(integer, 0_u8) == 0xFEDC'3211_u32);
        CHECK(bit::set(integer, 1_u8) == 0xFEDC'3212_u32);
        CHECK(bit::set(integer, 5_u8) == 0xFEDC'3230_u32);
    }

    SUBCASE("clear") {
          CHECK(bit::clear(integer, 4_u8) == 0xFEDC'3200_u32);
          CHECK(bit::clear(integer, 31_u8) == 0x7EDC'3210_u32);
          CHECK(bit::clear(integer, 27_u8) == 0xF6DC'3210_u32);
    }
}

TEST_CASE("mask ops")
{
    u16 integer = 0x0FF0_u16;

    SUBCASE("set") {
        CHECK(mask::set(integer, 0xF000_u16) == 0xFFF0_u16);
        CHECK(mask::set(integer, 0x000F_u16) == 0x0FFF_u16);
    }

    SUBCASE("clear") {
        CHECK(mask::clear(integer, 0x0F00_u16) == 0x00F0_u16);
        CHECK(mask::clear(integer, 0x00F0_u16) == 0x0F00_u16);
    }
}

TEST_CASE("math ops")
{
    u16 integer = 0x00F0_u16;

    SUBCASE("sign_extend") {
        CHECK(math::sign_extend<8>(integer) == 0xFFF0_i16);
        CHECK(math::sign_extend<12>(integer) == 0x00F0_i16);
    }

    SUBCASE("logical_shift_left") {
        CHECK(math::logical_shift_left(integer, 4_u8).result == 0x0F00_u16);
        CHECK(math::logical_shift_left(integer, 4_u8).carry == 0_u16);

        CHECK(math::logical_shift_left(integer, 9_u8).result == 0xE000_u16);
        CHECK(math::logical_shift_left(integer, 9_u8).carry == 1_u16);
    }

    SUBCASE("logical_shift_right") {
        CHECK(math::logical_shift_right(integer, 4_u8).result == 0x000F_u16);
        CHECK(math::logical_shift_right(integer, 4_u8).carry == 0_u16);

        CHECK(math::logical_shift_right(integer, 5_u8).result == 0x0007_u16);
        CHECK(math::logical_shift_right(integer, 5_u8).carry == 1_u16);
    }

    SUBCASE("arithmetic_shift_right") {
        u16 i1 = 0xF000_u16;
        CHECK(math::arithmetic_shift_right(i1, 4_u8).result == 0xFF00_u16);
        CHECK(math::arithmetic_shift_right(i1, 4_u8).carry == 0_u16);

        u16 i2 = 0x7FFF_u16;
        CHECK(math::arithmetic_shift_right(i2, 4_u8).result == 0x07FF_u16);
        CHECK(math::arithmetic_shift_right(i2, 4_u8).carry == 1_u16);
    }

    SUBCASE("logical_rotate_right") {
        CHECK(math::logical_rotate_right(integer, 4_u8).result == 0x000F_u16);
        CHECK(math::logical_rotate_right(integer, 4_u8).carry == 0_u16);

        CHECK(math::logical_rotate_right(integer, 5_u8).result == 0x8007_u16);
        CHECK(math::logical_rotate_right(integer, 5_u8).carry == 1_u16);
    }

    SUBCASE("logical_rotate_right_extended") {
        CHECK(math::logical_rotate_right_extended(integer, u16(1_u16)).result == 0x8078_u16);
        CHECK(math::logical_rotate_right_extended(integer, u16(1_u16)).carry == 0_u16);

        CHECK(math::logical_rotate_right_extended(integer, u16(0_u16)).result == 0x0078_u16);
        CHECK(math::logical_rotate_right_extended(integer, u16(0_u16)).carry == 0_u16);
    }
}
