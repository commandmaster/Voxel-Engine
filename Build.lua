workspace "Voxel Engine"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "App"

   outputdir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

   -- Compiler-specific flags
   filter { "system:windows", "action:vs*" }

      buildoptions { "/std:c++20", "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }
      cppdialect "C++20"

   filter { "system:windows", "action:gmake*" }

      buildoptions { "-std=c++20", "-Wall", "-Wextra" }
      cppdialect "C++20"

	filter { "system:linux", "action:gmake2" }
      toolset "clang"
      buildoptions { "-fms-extensions" }
      linkoptions { "-fuse-ld=lld" }


   filter {}



group "Core"
   include "Core/Build-Core-External.lua"
group ""

include "App/Build-App.lua"


