#!lua
local output = "./build/" .. _ACTION

solution "c2play_solution"
   configurations { "Debug", "Release" }

project "c2play"
   location (output)
   kind "ConsoleApp"
   language "C++"
   includedirs { "src/Media", "src/UI", "src/UI/Fbdev" }
   files { "src/**.h", "src/**.cpp" }
   excludes { "src/UI/X11/**" }
   buildoptions { "-std=c++14 -Wall" }
   linkoptions { "-lavformat -lavcodec -lavutil -lass -lasound -lrt -lpthread -lEGL -lGLESv2" }

   configuration "Debug"
      flags { "Symbols" }
      defines { "DEBUG", "EGL_NO_X11" }

   configuration "Release"
      flags { "Optimize" }
      defines { "EGL_NO_X11" }

project "c2play-x11"
   location (output)
   kind "ConsoleApp"
   language "C++"
   includedirs { "src/Media", "src/UI", "src/UI/X11", "/usr/include/dbus-c++-1" }
   files { "src/**.h", "src/**.cpp" }
   excludes { "src/UI/Fbdev/**" }
   buildoptions { "-std=c++14 -Wall" }
   linkoptions { "-lavformat -lavcodec -lavutil -lass -lasound -lrt -lpthread -lxcb -lxcb-image -lxcb-dpms -lxcb-screensaver -lEGL -lGLESv2 -ldbus-c++-1" }
   defines { "X11" }

   configuration "Debug"
      flags { "Symbols" }
      defines { "DEBUG" }

   configuration "Release"
      flags { "Optimize" }
      defines { }
