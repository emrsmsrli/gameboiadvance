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

std::optional<vector<u8>> compress(const vector<u8>& uncompressed) noexcept
{
    z_stream stream = make_z_stream();

    if(const int status = deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
      status != Z_OK) {
        LOG_ERROR(fs, "gzip deflateInit2: {}, file: {}", zError(status), path.string());
        return std::nullopt;
    }

    // reserve at least as much size as `uncompressed` has
    vector<u8> compressed = uncompressed;

    stream.next_in = reinterpret_cast<const Bytef*>(uncompressed.data()); // NOLINT
    stream.avail_in = uncompressed.size().get();
    stream.next_out = reinterpret_cast<Bytef*>(compressed.data()); // NOLINT
    stream.avail_out = compressed.size().get();

    if(const int status = deflate(&stream, Z_FINISH); status < Z_OK) {
        LOG_ERROR(fs, "gzip deflate: {}, file: {}", zError(status), path.string());
        return std::nullopt;
    }

    compressed.resize(stream.total_out);
    deflateEnd(&stream);

    return compressed;
}

std::optional<vector<u8>> uncompress(const vector<u8>& compressed) noexcept
{
    z_stream stream = make_z_stream();

    if(const int status = inflateInit2(&stream, 16); status != Z_OK) {
        LOG_ERROR(fs, "gzip inflateInit2: {}, file: {}", zError(status), path.string());
        return std::nullopt;
    }

    // last 4 bytes encodes size modulo 2^32 (modulo can be ignored)
    const u32 uncompressed_size = memcpy<u32>(compressed, compressed.size() - 4_usize);

    vector<u8> uncompressed{uncompressed_size};
    stream.next_in = reinterpret_cast<const Bytef*>(compressed.data()); // NOLINT
    stream.avail_in = compressed.size().get();
    stream.next_out = reinterpret_cast<Bytef*>(uncompressed.data()); // NOLINT
    stream.avail_out = uncompressed.size().get();

    if(const int status = inflate(&stream, Z_FINISH); status < Z_OK) {
        LOG_ERROR(fs, "gzip inflate: {}, file: {}", zError(status), path.string());
        return std::nullopt;
    }

    inflateEnd(&stream);
    return uncompressed;
}

} // namespace gba::gzip
