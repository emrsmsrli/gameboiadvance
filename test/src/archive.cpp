/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <algorithm>

#include <gba/archive.h>
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
};

} // namespace

TEST_CASE("state archive serialize")
{
    archive archive;

    SUBCASE("integer") {
        static constexpr i32 i = 12;
        archive.serialize(i);
#if DEBUG
        static constexpr array<u8, 5> verification{
            from_enum<u8>(archive::state_type::integer), 0x0c_u8, 0x00_u8, 0x00_u8, 0x00_u8
        };
#else
        static constexpr array<u8, 4> verification{0x0c_u8, 0x00_u8, 0x00_u8, 0x00_u8};
#endif // DEBUG

        REQUIRE(archive.data().size() == verification.size());
        REQUIRE(std::equal(archive.data().begin(), archive.data().end(), verification.begin(), verification.end()));
    }

    SUBCASE("array") {
        static constexpr array<dummy, 2> dummies{
            dummy{0x00110011_u32, 0xFF_u8},
            dummy{0x22334455_u32, 0xDD_u8}
        };

        archive.serialize(dummies);
#if DEBUG
        static constexpr array<u8, 15> verification{
            from_enum<u8>(archive::state_type::array),
            from_enum<u8>(archive::state_type::integer), 0x11_u8, 0x00_u8, 0x11_u8, 0x00_u8,
            from_enum<u8>(archive::state_type::integer), 0xFF_u8,
            from_enum<u8>(archive::state_type::integer), 0x55_u8, 0x44_u8, 0x33_u8, 0x22_u8,
            from_enum<u8>(archive::state_type::integer), 0xDD_u8
        };
#else
        static constexpr array<u8, 10> verification{
            0x11_u8, 0x00_u8, 0x11_u8, 0x00_u8, 0xFF_u8,
            0x55_u8, 0x44_u8, 0x33_u8, 0x22_u8, 0xDD_u8
        };
#endif // DEBUG

        REQUIRE(archive.data().size() == verification.size());
        REQUIRE(std::equal(archive.data().begin(), archive.data().end(), verification.begin(), verification.end()));
    }

    SUBCASE("array of bytes") {
        static constexpr array<u8, 4> dummies{0x11_u8, 0xFD_u8, 0x45_u8, 0x98_u8};
        archive.serialize(dummies);
#if DEBUG
        static constexpr array<u8, 9> verification{
            from_enum<u8>(archive::state_type::array),
            from_enum<u8>(archive::state_type::integer), 0x11_u8,
            from_enum<u8>(archive::state_type::integer), 0xFD_u8,
            from_enum<u8>(archive::state_type::integer), 0x45_u8,
            from_enum<u8>(archive::state_type::integer), 0x98_u8
        };
#else
        static constexpr array<u8, 4> verification = dummies;
#endif // DEBUG

        REQUIRE(archive.data().size() == verification.size());
        REQUIRE(std::equal(archive.data().begin(), archive.data().end(), verification.begin(), verification.end()));
    }

    SUBCASE("vector") {
        vector<dummy> dummies;
        dummies.push_back(dummy{0x00110011_u32, 0xFF_u8});
        dummies.push_back(dummy{0x22334455_u32, 0xDD_u8});

        archive.serialize(dummies);
#if DEBUG
        static constexpr array<u8, 24> verification{
          from_enum<u8>(archive::state_type::vector),
          from_enum<u8>(archive::state_type::integer), 0x02_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8,
          from_enum<u8>(archive::state_type::integer), 0x11_u8, 0x00_u8, 0x11_u8, 0x00_u8,
          from_enum<u8>(archive::state_type::integer), 0xFF_u8,
          from_enum<u8>(archive::state_type::integer), 0x55_u8, 0x44_u8, 0x33_u8, 0x22_u8,
          from_enum<u8>(archive::state_type::integer), 0xDD_u8
        };
#else
        static constexpr array<u8, 18> verification{
            0x02_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8,
            0x11_u8, 0x00_u8, 0x11_u8, 0x00_u8, 0xFF_u8,
            0x55_u8, 0x44_u8, 0x33_u8, 0x22_u8, 0xDD_u8
        };
#endif // DEBUG

          REQUIRE(archive.data().size() == verification.size());
          REQUIRE(std::equal(archive.data().begin(), archive.data().end(), verification.begin(), verification.end()));
    }

    SUBCASE("vector of bytes") {
        vector<u8> dummies;
        dummies.push_back(0x11_u8);
        dummies.push_back(0xFD_u8);
        dummies.push_back(0x45_u8);
        dummies.push_back(0x98_u8);

        archive.serialize(dummies);
#if DEBUG
        static constexpr array<u8, 18> verification{
          from_enum<u8>(archive::state_type::vector),
          from_enum<u8>(archive::state_type::integer), 0x04_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8,
          from_enum<u8>(archive::state_type::integer), 0x11_u8,
          from_enum<u8>(archive::state_type::integer), 0xFD_u8,
          from_enum<u8>(archive::state_type::integer), 0x45_u8,
          from_enum<u8>(archive::state_type::integer), 0x98_u8
        };
#else
        static constexpr array<u8, 12> verification{
            0x04_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8, 0x00_u8,
            0x11_u8,
            0xFD_u8,
            0x45_u8,
            0x98_u8
        };
#endif // DEBUG

          REQUIRE(archive.data().size() == verification.size());
          REQUIRE(std::equal(archive.data().begin(), archive.data().end(), verification.begin(), verification.end()));
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
        REQUIRE(std::equal(dummies.begin(), dummies.end(), other_dummies.begin(), other_dummies.end()));
    }

    SUBCASE("vector") {
        vector<dummy> dummies;
        dummies.push_back(dummy{0x00110011_u32, 0xFF_u8});
        dummies.push_back(dummy{0x22334455_u32, 0xDD_u8});
        archive.serialize(dummies);

        vector<dummy> other_dummies;
        archive.deserialize(other_dummies);
        REQUIRE(dummies.size() == other_dummies.size());
        REQUIRE(std::equal(dummies.begin(), dummies.end(), other_dummies.begin(), other_dummies.end()));
    }
}
