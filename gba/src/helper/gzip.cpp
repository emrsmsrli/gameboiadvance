/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/helper/gzip.h>

#define ZLIB_CONST
#include <zlib.h>

namespace gba::gzip {

namespace {

z_stream make_z_stream() noexcept
{
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    return stream;
}

} // namespace

std::optional<vector<u8>> compress(const vector<u8>& decompressed) noexcept
{
    z_stream stream = make_z_stream();

    if(const int status = deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
      status != Z_OK) {
        LOG_ERROR(fs, "gzip deflateInit2: {}", zError(status));
        return std::nullopt;
    }

    // reserve at least as much size as `decompressed` has
    vector<u8> compressed = decompressed;

    stream.next_in = reinterpret_cast<const Bytef*>(decompressed.data()); // NOLINT
    stream.avail_in = decompressed.size().get();
    stream.next_out = reinterpret_cast<Bytef*>(compressed.data()); // NOLINT
    stream.avail_out = compressed.size().get();

    if(const int status = deflate(&stream, Z_FINISH); status < Z_OK) {
        LOG_ERROR(fs, "gzip deflate: {}", zError(status));
        return std::nullopt;
    }

    compressed.resize(stream.total_out);
    deflateEnd(&stream);

    return compressed;
}

std::optional<vector<u8>> decompress(const vector<u8>& compressed) noexcept
{
    z_stream stream = make_z_stream();

    if(const int status = inflateInit2(&stream, 32 + MAX_WBITS); status != Z_OK) {
        LOG_ERROR(fs, "gzip inflateInit2: {}", zError(status));
        return std::nullopt;
    }

    // last 4 bytes encodes size modulo 2^32 (modulo can be ignored)
    const u32 decompressed_size = memcpy<u32>(compressed, compressed.size() - 4_usize);

    vector<u8> decompressed{decompressed_size};
    stream.next_in = reinterpret_cast<const Bytef*>(compressed.data()); // NOLINT
    stream.avail_in = compressed.size().get();
    stream.next_out = reinterpret_cast<Bytef*>(decompressed.data()); // NOLINT
    stream.avail_out = decompressed.size().get();

    if(const int status = inflate(&stream, Z_FINISH); status < Z_OK) {
        LOG_ERROR(fs, "gzip inflate: {}", zError(status));
        return std::nullopt;
    }

    inflateEnd(&stream);
    return decompressed;
}

} // namespace gba::gzip
