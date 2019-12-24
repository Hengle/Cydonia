#include <HID/InputInterpreter.h>

#include <Common/Assert.h>

#include <Window/GLFWWindow.h>

#include <HID/Controller.h>

#include <GLFW/glfw3.h>

namespace cyd
{
InputInterpreter::InputInterpreter( const Window& window ) : _window( window )
{
   // Settings instance of input interpreter to this window
   // TODO Maybe there's a better way to do this?
   glfwSetWindowUserPointer( _window.getGLFWwindow(), this );

   // Callback wrappers
   auto mainKeyCallback = []( GLFWwindow* window, int key, int scancode, int action, int mods ) {
      static_cast<InputInterpreter*>( glfwGetWindowUserPointer( window ) )
          ->mainKeyCallback( window, key, scancode, action, mods );
   };

   auto mainCursorCallback = []( GLFWwindow* window, double xpos, double ypos ) {
      static_cast<InputInterpreter*>( glfwGetWindowUserPointer( window ) )
          ->mainCursorCallback( window, xpos, ypos );
   };

   auto mainMouseCallback = []( GLFWwindow* window, int button, int action, int mods ) {
      static_cast<InputInterpreter*>( glfwGetWindowUserPointer( window ) )
          ->mainMouseCallback( window, button, action, mods );
   };

   // Registering callbacks
   glfwSetKeyCallback( _window.getGLFWwindow(), mainKeyCallback );
   glfwSetCursorPosCallback( _window.getGLFWwindow(), mainCursorCallback );
   glfwSetMouseButtonCallback( _window.getGLFWwindow(), mainMouseCallback );
}

void InputInterpreter::tick()
{
   glfwPollEvents();
   for( const auto& controller : _controllers )
   {
      controller->interpret();
   }
}

void InputInterpreter::mainKeyCallback(
    GLFWwindow* window,
    int key,
    int scancode,
    int action,
    int mods )
{
   if( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
   {
      glfwSetWindowShouldClose( window, true );
   }

   for( const auto& controller : _controllers )
   {
      controller->keyCallback( window, key, scancode, action, mods );
   }
}

void InputInterpreter::mainCursorCallback( GLFWwindow* window, double xpos, double ypos )
{
   for( const auto& controller : _controllers )
   {
      controller->cursorCallback( window, xpos, ypos );
   }
}

void InputInterpreter::mainMouseCallback( GLFWwindow* window, int button, int action, int mods )
{
   for( const auto& controller : _controllers )
   {
      controller->mouseCallback( window, button, action, mods );
   }
}
}