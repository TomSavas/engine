cmake_minimum_required(VERSION 3.1)

set(PROJECT engine)
project(${PROJECT})
set(EXEC ${PROJECT})

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0 -DDEBUG -lvulkan -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -lvulkan -Wall")

file(GLOB_RECURSE SOURCES "engine/src/*.cpp" "lib/imgui/*.cpp" "lib/SPIRV-Reflect/spirv_reflect.cpp")

add_executable(${EXEC} ${SOURCES})
target_include_directories(${PROJECT} PRIVATE engine/src/)
target_include_directories(${PROJECT} PRIVATE engine/include/)

target_include_directories(${PROJECT} PRIVATE lib/imgui)
target_include_directories(${PROJECT} PRIVATE lib/tinygltf)
target_include_directories(${PROJECT} PRIVATE lib/tinyobjloader)
target_include_directories(${PROJECT} PRIVATE lib/stb_image)
target_include_directories(${PROJECT} PRIVATE lib/SPIRV-Reflect)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

target_include_directories(${PROJECT} PRIVATE lib/VulkanMemoryAllocator/include)

find_package(Vulkan REQUIRED FATAL_ERROR)
target_link_libraries(${PROJECT} Vulkan::Vulkan)

add_subdirectory(lib/glfw)
target_link_libraries(${PROJECT} glfw)

add_subdirectory(lib/vk-bootstrap)
target_link_libraries(${PROJECT} vk-bootstrap::vk-bootstrap)

add_subdirectory(lib/glm)
target_link_libraries(${PROJECT} glm::glm)

add_definitions(-DGLFW_INCLUDE_NONE)

find_program(GLSLC glslc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/engine/shaders/*.glsl"
)

set(SPV_MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SPV_MAKE_DIRECTORY}")

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${SPV_MAKE_DIRECTORY}/${FILE_NAME}.spv")

    string(REGEX MATCH "\\.[^\.]*\\.glsl" SHADER_STAGE ${GLSL})
    string(REGEX REPLACE "\\.glsl" "" SHADER_STAGE ${SHADER_STAGE})
    string(REGEX REPLACE "\\." "" SHADER_STAGE ${SHADER_STAGE})

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC} -fshader-stage=${SHADER_STAGE} ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        COMMENT "Compiling ${GLSL} for stage: ${SHADER_STAGE}"
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    shaders
    ALL
    DEPENDS ${SPIRV_BINARY_FILES}
)
