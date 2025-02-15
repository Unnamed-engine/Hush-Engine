# Rendering

hush_add_library(
        TARGET_NAME HushRendering
        LIB_TYPE OBJECT
        SRCS src/WindowRenderer.cpp
             src/Vulkan/VulkanAllocatedBuffer.cpp
             src/Vulkan/VulkanRenderer.cpp
             src/Vulkan/GltfMetallicRoughness.cpp
             src/Vulkan/VulkanMeshNode.cpp
             src/Vulkan/VulkanPipelineBuilder.cpp
             src/Vulkan/VulkanLoader.cpp
             src/Vulkan/VkDescriptors.cpp
             src/Vulkan/VulkanSwapchain.cpp
             src/Shared/Camera.cpp
             src/Shared/ImageTexture.cpp
             src/Shared/EditorCamera.cpp
             src/Shared/GltfLoadFunctions.cpp
             src/ImGui/VulkanImGuiForwarder.cpp
        PUBLIC_HEADER_DIRS src
)
add_library(Hush::Rendering ALIAS HushRendering)

target_link_libraries(HushRendering PUBLIC
        SDL2::SDL2
        imgui
        stb
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        glm::glm
        fastgltf::fastgltf
        HushLog
        HushUtils
        HushInput
)

