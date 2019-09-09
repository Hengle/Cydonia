// Tell SDL not to mess with main()
#define SDL_MAIN_HANDLED

#include "Window.h"

#include "Common/Assert.h"

#include <vulkan/vulkan.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

static void handleSDLError()
{
   const char* error = SDL_GetError();
   CYDASSERT( !error );
}

cyd::Window::Window( uint32_t width, uint32_t height, const std::string& title )
    : _width( width ), _height( height )
{
   if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) handleSDLError();

   // Creating SDL_Window
   _sdlWindow = SDL_CreateWindow(
       title.c_str(),
       SDL_WINDOWPOS_CENTERED,
       SDL_WINDOWPOS_CENTERED,
       _width,
       _height,
       SDL_WINDOW_VULKAN );
   CYDASSERT( _sdlWindow && "Could not create SDL window" );

   // Populating extensions
   uint32_t extensionsCount = 0;
   if( !SDL_Vulkan_GetInstanceExtensions( _sdlWindow, &extensionsCount, nullptr ) )
   {
      handleSDLError();
   }

   _extensions.resize( extensionsCount );

   if( !SDL_Vulkan_GetInstanceExtensions( _sdlWindow, &extensionsCount, _extensions.data() ) )
   {
      handleSDLError();
   }

#if _DEBUG
   _extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
#endif
}

cyd::Window::~Window()
{
   SDL_DestroyWindow( _sdlWindow );
   SDL_Quit();
}
