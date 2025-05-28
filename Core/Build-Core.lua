project "Core"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files 
   {
        "source/**.h", "source/**.cpp", "source/**.hpp" ,
        "../Vendor/imgui/backends/imgui_impl_glfw.cpp",
        "../Vendor/imgui/backends/imgui_impl_vulkan.cpp",
}

   includedirs
   {
        "source",

        "../Vendor/imgui",
        "../Vendor/glfw/include",
        "../Vendor/entt",

        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.shaderc}",
        "%{IncludeDir.vma}",
   }

   libdirs { "%{LibraryDir.VulkanSDK}" }

    links
    {
          "ImGui",
          "GLFW",
    
          "%{Library.Vulkan}",
    }

   

   targetdir ("../Binaries/" .. outputdir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. outputdir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines { }

   filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"
        links
        {
            "%{Library.shadercDebug}",
        }

   filter "configurations:Release"
        defines { "RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "On"
        links
        {
            "%{Library.shaderc}",
        }
        

   filter "configurations:Dist"
        defines { "DIST" }
        runtime "Release"
        optimize "On"
        symbols "Off"
        links
        {
            "%{Library.shaderc}",
        }    