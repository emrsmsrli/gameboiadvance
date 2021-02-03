/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <test_prelude.h>

#include <chrono>
#include <ctime>

#include <gba/cartridge/rtc.h>

using namespace gba;
using namespace gba::cartridge;

namespace {

constexpr array<u8, 8> make_bitstream(const u8 data)
{
    array<u8, 8> stream;
    for(usize i = 0_usize; i < stream.size(); ++i) {
        stream[i] = bit::extract(data, narrow<u8>(i));
    }

    return stream;
}

}

TEST_CASE("rtc cmds")
{
    const auto write_bitstream = [](rtc& r, u8 byte) {
        r.write(rtc::port_data, 0b0001_u8);
        r.write(rtc::port_data, 0b0101_u8);
        for(u8 bit : make_bitstream(byte)) {
            r.write(rtc::port_data, 0b0100_u8 | (bit << 1_u8));
            r.write(rtc::port_data, 0b0101_u8);
        }
    };

    rtc r;

    SUBCASE("gpio read allowance") {
        CHECK(!r.read_allowed());
        r.write(rtc::port_control, 0b1_u8);
        CHECK(r.read_allowed());
        r.write(rtc::port_control, 0b0_u8);
        CHECK(!r.read_allowed());
    }

    SUBCASE("read date-time") {
        r.write(rtc::port_control, 0b1_u8);
        r.write(rtc::port_direction, 0b1111_u8);

        write_bitstream(r, 0b0000'0110_u8); // reset (no-op)
        write_bitstream(r, 0b1010'0110_u8); // read date-time

        r.write(rtc::port_direction, 0b1101_u8);

        array<u8, 7> received_bytes;
        for(usize i = 0_usize; i < 7_usize; ++i) {
            for(usize b = 0_usize; b < 8_usize; ++b) {
                r.write(rtc::port_data, 0b0101_u8);
                received_bytes[i] |= bit::extract(r.read(rtc::port_data), 1_u8) << narrow<u8>(b);
            }
        }

        using namespace std::chrono;
        const std::time_t t = system_clock::to_time_t(system_clock::now());
        const std::tm* date = std::localtime(&t);

        const auto to_bcd = [](auto data) {
            u8 d = static_cast<u8::type>(data);
            const u8 counter = d % 10_u8;
            d /= 10_u8;
            return counter + ((d % 10_u8) << 4_u8);
        };

        CHECK(to_bcd(date->tm_year - 100) == received_bytes[0_usize]);
        CHECK(to_bcd(date->tm_mon + 1) == received_bytes[1_usize]);
        CHECK(to_bcd(date->tm_mday) == received_bytes[2_usize]);
        CHECK(to_bcd(date->tm_wday) == received_bytes[3_usize]);
        CHECK(to_bcd(date->tm_hour) == received_bytes[4_usize]);
        CHECK(to_bcd(date->tm_min) == received_bytes[5_usize]);
        CHECK(to_bcd(date->tm_sec) == received_bytes[6_usize]);
    }
}
