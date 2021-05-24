#include "sdl2cpp/sdl_audio.h"

#include <SDL2/SDL_audio.h>
#include <spdlog/spdlog.h>

#include "sdl2cpp/sdl_macro.h"

namespace sdl {

size_t audio_device::num_devices() noexcept
{
    return SDL_GetNumAudioDevices(SDL_FALSE);
}

std::string_view audio_device::device_name(const int32_t index) noexcept
{
    return SDL_GetAudioDeviceName(index, SDL_FALSE);
}

audio_device::audio_device(const char* device_name, uint8_t channels, audio_device::format format,
  uint32_t sampling_rate, uint16_t sample_count) noexcept
{
    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.channels = channels;
    desired.format = static_cast<SDL_AudioFormat>(format);
    desired.freq = sampling_rate;
    desired.samples = sample_count;

    SDL_AudioSpec obtained;
    SDL_zero(obtained);

    device_id_ = SDL_OpenAudioDevice(device_name, SDL_FALSE, &desired, &obtained,
      SDL_AUDIO_ALLOW_SAMPLES_CHANGE | SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    SDL_CHECK(device_id_);

    frequency_ = obtained.freq;
    sample_count_ = obtained.samples;

    SPDLOG_TRACE("opened audio device: {}, id: {}, freq: {}, samples: {}",
      device_name != nullptr ? device_name : "default", device_id_, frequency_, sample_count_);
}

audio_device::~audio_device() noexcept
{
    if(device_id_ != invalid_id) {
        SPDLOG_TRACE("closing audio device with id {}", device_id_);
        SDL_CloseAudioDevice(device_id_);
    }
}

audio_device& audio_device::operator=(audio_device&& other) noexcept
{
    device_id_ = std::exchange(other.device_id_, invalid_id);
    return *this;
}

void audio_device::resume() noexcept
{
    SDL_PauseAudioDevice(device_id_, SDL_FALSE);
}

void audio_device::pause() noexcept
{
    SDL_PauseAudioDevice(device_id_, SDL_TRUE);
}

void audio_device::enqueue(const void* data, size_t size_in_bytes) noexcept
{
    SDL_QueueAudio(device_id_, data, size_in_bytes);
}

size_t audio_device::queue_size() noexcept
{
    return SDL_GetQueuedAudioSize(device_id_);
}

} // namespace sdl
