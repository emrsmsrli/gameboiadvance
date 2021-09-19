/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <algorithm>

#include <gba/archive.h>
#include <gba/helper/range.h>
#include <test_prelude.h>

using namespace gba;

namespace {

struct dummy {
    u32 u;
    u8 b;

    void serialize(archive& a) const noexcept
    {
        a.serialize(u);
        a.serialize(b);
    }

    void deserialize(const archive& a) noexcept
    {
        a.deserialize(u);
        a.deserialize(b);
    }

    bool operator==(const dummy& other) const noexcept { return u == other.u && b == other.b; }
    bool operator!=(const dummy& other) const noexcept { return !(*this == other); }
};

template<typename T1, typename T2>
bool verify_same(T1&& t1, T2&& t2) noexcept
{
    if(t1.size() != t2.size()) { return false; }

    for(usize idx : range(t1.size())) {
        if(t1[idx] != t2[idx]) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("state archive serialize")
{
    archive archive;

    SUBCASE("integer") {
        static constexpr i32 i = 12;
        archive.serialize(i);

        static constexpr array<u8, 4> verification{0x0c_u8, 0x00_u8, 0x00_u8, 0x00_u8};

        REQUIRE(verify_same(archive.data(), verification));
    }

    SUBCASE("array") {
        static constexpr array<dummy, 2> dummies{
            dummy{0x00110011_u32, 0xFF_u8},
            dummy{0x22334455_u32, 0xDD_u8}
        };

        archive.serialize(dummies);

        static constexpr array<u8, 10> verification{
            0x11_u8, 0x00_u8, 0x11_u8, 0x00_u8, 0xFF_u8,
            0x55_u8, 0x44_u8, 0x33_u8, 0x22_u8, 0xDD_u8
        };

        REQUIRE(verify_same(archive.data(), verification));
    }

    SUBCASE("array of bytes") {
        static constexpr array<u8, 4> dummies{0x11_u8, 0xFD_u8, 0x45_u8, 0x98_u8};
        archive.serialize(dummies);

        static constexpr array<u8, 4> verification = dummies;

        REQUIRE(verify_same(archive.data(), verification));
    }

    SUBCASE("vector") {
        vector<dummy> dummies;
        dummies.push_back(dummy{0x00110011_u32, 0xFF_u8});
        dummies.push_back(dummy{0x22334455_u32, 0xDD_u8});

        archive.serialize(dummies);

        static constexpr array<u8, 18> verification{
            0x02_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8,
            0x11_u8, 0x00_u8, 0x11_u8, 0x00_u8, 0xFF_u8,
            0x55_u8, 0x44_u8, 0x33_u8, 0x22_u8, 0xDD_u8
        };

        REQUIRE(verify_same(archive.data(), verification));
    }

    SUBCASE("vector of bytes") {
        vector<u8> dummies;
        dummies.push_back(0x11_u8);
        dummies.push_back(0xFD_u8);
        dummies.push_back(0x45_u8);
        dummies.push_back(0x98_u8);

        archive.serialize(dummies);

        static constexpr array<u8, 12> verification{
            0x04_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8,
            0x11_u8,
            0xFD_u8,
            0x45_u8,
            0x98_u8
        };

        REQUIRE(verify_same(archive.data(), verification));
    }
}

TEST_CASE("state archive deserialize")
{
    archive archive;

    SUBCASE("integer") {
        static constexpr i32 i = 12;
        archive.serialize(i);

        i32 other_i;
        archive.deserialize(other_i);
        REQUIRE(i == other_i);
    }

    SUBCASE("array") {
        static constexpr array<dummy, 2> dummies{
            dummy{0x00110011_u32, 0xFF_u8},
            dummy{0x22334455_u32, 0xDD_u8}
        };
        archive.serialize(dummies);

        array<dummy, 2> other_dummies;
        archive.deserialize(other_dummies);
        REQUIRE(verify_same(dummies, other_dummies));
    }

    SUBCASE("vector") {
        vector<dummy> dummies;
        dummies.push_back(dummy{0x00110011_u32, 0xFF_u8});
        dummies.push_back(dummy{0x22334455_u32, 0xDD_u8});
        archive.serialize(dummies);

        vector<dummy> other_dummies;
        archive.deserialize(other_dummies);
        REQUIRE(verify_same(dummies, other_dummies));
    }
}
