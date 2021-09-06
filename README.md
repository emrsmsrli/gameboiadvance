<img src=".github/logo/logo.png" />

# gameboi advance

[![Build Status](https://travis-ci.com/emrsmsrli/gameboiadvance.svg?branch=master)](https://travis-ci.com/emrsmsrli/gameboiadvance)
[![GitHub license](https://img.shields.io/github/license/emrsmsrli/gameboiadvance)](https://github.com/emrsmsrli/gameboiadvance/blob/master/LICENSE)

A Game Boy Advance emulator built upon the GBC emulator [gameboi](https://github.com/emrsmsrli/gameboi/)

## Features

gameboiadvance does not aim to be the perfect emulator,
its features are limited compared to other emulators.

- Accurate ARM7 emulation
- Scanline based PPU emulation 
- APU emulation (based on [gameboi](https://github.com/emrsmsrli/gameboi/))
- Accurate DMA interleaving
- EEPROM, FLASH and SRAM save-load capability
- RTC support _(sadly, no other GPIO extensions)_
- Gamepak prefetch emulation
- Disassembler and a powerful debugger
  - Capable of showing all the internals of the emulator
  - Execution and data access breakpoint support
  
### Feature TODO
- SIO emulation, and networked multiplayer support

## Screenshots

<p align="center">
    <img src=".github/screenshots/zelda.png" height=200 />
    <img src=".github/screenshots/pokeemerald.png" height=200 />
    <img src=".github/screenshots/wario.png" height=200 />
</p>
<p align="center">
    <img src=".github/screenshots/doom.png" height=200 />
    <img src=".github/screenshots/advancewars.png" height=200 />
    <img src=".github/screenshots/supermario.png" height=200 />
</p>
<p align="center">
    <img src=".github/screenshots/debugger.png" width=800 />
</p>

## Using as a library

gameboiadvance library can be easily integrated to your own frontend implementation:
```cpp
#include <gba/core.h>

// WITH_DEBUGGER can be set with CMake argument -DWITH_DEBUGGER=ON
#if WITH_DEBUGGER
  #include <gba_debugger/debugger.h>
#endif // WITH_DEBUGGER

void on_scanline(const gba::u8 line_number, const gba::ppu::scanline_buffer& buffer) noexcept;
void on_vblank() noexcept;
void on_audio(const gba::vector<gba::apu::stereo_sample<float>>& buffer) noexcept;

int main(int argc, char* argv[]) 
{
    // BIOS is required to boot
    gba::core core{"file/path/to/bios.gba"};
    core.load_pak("file/path/to/rom.gba");

#if WITH_DEBUGGER
    gba::debugger::window debugger_window(&core);

    while(true) {
        // debugger window handles input, video and audio output
        debugger_window.tick();
    }
#else
    core.on_scanline_event().add_delegate(gba::connect_arg<&on_scanline>);
    core.on_vblank_event().add_delegate(gba::connect_arg<&on_vblank>);
    core.sound_buffer_overflow_event().add_delegate(gba::connect_arg<&on_audio>);

    // optionally set audio generation parameters
    // core.set_dst_sample_rate(some_audio_device.sample_rate());
    // core.set_sound_buffer_capacity(some_audio_device.sample_count());

    while(true) {
        // core.press_key(gba::keypad::key::a);
        // core.release_key(gba::keypad::key::start);

        core.tick_one_frame();
        // or core.tick(n); which executes at least `n` cycles every iteration
    }
#endif // WITH_DEBUGGER

    return 0;
}
```

## Compiling
### Dependencies
- CMake 3.16
- fmt (as git submodule)
- spdlog (as git submodule)
- SFML
- SDL2 (for sound only)

Install dependencies however you like. Below example is installing using vcpkg, in Ubuntu:

```shell
$ git clone https://github.com/Microsoft/vcpkg.git
$ ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
$ ./vcpkg/vcpkg install sfml sdl2
$ sudo apt install cmake
```

### Compile using CMake
gameboiadvance uses CMake and can be easily built with a script like below.

```shell
$ mkdir build && cd build
$ cmake --config Release --target gameboiadvance ..
$ cmake --build -- -j $(nproc)
```
