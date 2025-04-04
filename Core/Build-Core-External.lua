VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["glm"] = "../Vendor/glm"
IncludeDir["shaderc"] = "%{VULKAN_SDK}/Include/shaderc"
IncludeDir["vma"] = "../Vendor/vma"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"


Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["shaderc"] = "%{LibraryDir.VULKAN_SDK}/shaderc_combined.lib"
Library["shadercDebug"] = "%{LibraryDir.VULKAN_SDK}/shaderc_combinedd.lib"

group "Dependencies"
   include "../Vendor/imgui"
   include "../Vendor/glfw"
group ""

group "Core"
    include "./Build-Core.lua"
group ""