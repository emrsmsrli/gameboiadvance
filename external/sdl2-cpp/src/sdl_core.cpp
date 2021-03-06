#include "sdl2cpp/sdl_core.h"

#include <SDL2/SDL.h>

#include "sdl2cpp/sdl_macro.h"

namespace sdl {

void init() noexcept
{
    SDL_CHECK_INIT(SDL_Init(SDL_INIT_AUDIO));
}

void quit() noexcept
{
    SDL_Quit();
}

} // namespace sdl
