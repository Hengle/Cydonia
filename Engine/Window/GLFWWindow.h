#pragma once

// Enable the WSI extensions
#if defined( __linux__ )
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined( _WIN32 )
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <Graphics/GraphicsTypes.h>

#include <cstdint>
#include <memory>
#include <vector>

// ================================================================================================
// Forwards
// ================================================================================================
struct GLFWwindow;

// ================================================================================================
// Definition
// ================================================================================================
namespace CYD
{
class Window final
{
  public:
   Window() = default;
   ~Window();

   bool init( uint32_t width, uint32_t height, const char* title );

   bool isRunning() const;
   const Extent2D& getExtent() const noexcept { return m_extent; }
   GLFWwindow* getGLFWwindow() const noexcept { return m_glfwWindow; }
   std::vector<const char*> getExtensionsFromGLFW() const noexcept { return m_extensions; };

  private:
   std::vector<const char*> m_extensions;

   // Window is the owner of this GLFWwindow
   GLFWwindow* m_glfwWindow = nullptr;

   // Dimensions
   Extent2D m_extent = {0, 0};
};
}
