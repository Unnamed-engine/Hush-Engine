message("*****Compiling ImGui*****")

message("Getting dependencies...")
find_package(SDL2 REQUIRED)

set(BASE_INCLUDE_PATH "${CMAKE_CURRENT_LIST_DIR}/include/imgui")

file(GLOB IMGUI_BASE_SRC "${BASE_INCLUDE_PATH}/*.h" "${BASE_INCLUDE_PATH}/*.cpp")

set(IMGUI_SOURCES
	${BASE_INCLUDE_PATH}/backends/imgui_impl_sdl2.cpp
	${BASE_INCLUDE_PATH}/backends/imgui_impl_sdl2.h
	${BASE_INCLUDE_PATH}/backends/imgui_impl_vulkan.cpp
	${BASE_INCLUDE_PATH}/backends/imgui_impl_vulkan.h
	${IMGUI_BASE_SRC}
)

message("ImGui base sources")

add_library(imgui STATIC ${IMGUI_SOURCES})

message("Linking dependencies...")
target_link_libraries(imgui PUBLIC SDL2::SDL2)

target_include_directories(imgui PUBLIC include)

message("*****ImGui End*****")