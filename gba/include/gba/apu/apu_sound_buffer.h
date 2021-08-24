/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_APU_SOUND_BUFFER_H
#define GAMEBOIADVANCE_APU_SOUND_BUFFER_H

#include <gba/core/container.h>
#include <gba/core/event/event.h>

namespace gba::apu {

template<typename Sample>
class sound_buffer {
    usize capacity_ = 2048_usize;
    usize write_idx_;
    vector<Sample> buffer_{capacity_};

public:
#if WITH_DEBUGGER
    event<usize> on_write;
#endif // WITH_DEBUGGER

    event<vector<Sample>> on_overflow;

    FORCEINLINE void set_capacity(usize capacity) noexcept
    {
        capacity_ = capacity;
        buffer_.resize(capacity);
        write_idx_ = std::min(write_idx_, capacity);
        notify_on_overflow();
    }

    FORCEINLINE void write(const Sample& sample) noexcept
    {
#if WITH_DEBUGGER
        on_write(write_idx_);
#endif // WITH_DEBUGGER

        buffer_[write_idx_++] = sample;
        notify_on_overflow();
    }

private:
    FORCEINLINE void notify_on_overflow() noexcept
    {
        if(write_idx_ == capacity_) {
            write_idx_ = 0_usize;
            on_overflow(buffer_);
        }
    }
};

} // namespace gba::apu

#endif //GAMEBOIADVANCE_APU_SOUND_BUFFER_H
