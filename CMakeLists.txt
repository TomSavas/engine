cmake_minimum_required(VERSION 3.5)

set(PROJECT engine)
project(${PROJECT})
set(EXEC ${PROJECT})

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_FLAGS "-fno-omit-frame-pointer -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

file(GLOB_RECURSE SOURCES "engine/src/*.cpp")
file(GLOB EXTERNAL_SOURCES
    "lib/imgui/*.cpp"
    "lib/imgui/backends/imgui_impl_glfw.cpp"
    "lib/imgui/backends/imgui_impl_vulkan.cpp"
    "lib/SPIRV-Reflect/spirv_reflect.cpp")
list(APPEND SOURCES ${EXTERNAL_SOURCES})

add_executable(${EXEC} ${SOURCES})
# set_target_properties(${PROJECT} PROPERTIES UNITY_BUILD ON UNITY_BUILD_MODE BATCH)

target_include_directories(${PROJECT} PRIVATE engine/src/)
target_include_directories(${PROJECT} PRIVATE engine/include/)

target_include_directories(${PROJECT} PRIVATE lib/imgui)
target_include_directories(${PROJECT} PRIVATE lib/imgui/backends)
target_include_directories(${PROJECT} PRIVATE lib/tinygltf)
target_include_directories(${PROJECT} PRIVATE lib/tinyobjloader)
target_include_directories(${PROJECT} PRIVATE lib/stb_image)
target_include_directories(${PROJECT} PRIVATE lib/SPIRV-Reflect)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT} Vulkan::Vulkan)

add_subdirectory(lib/glfw)
target_link_libraries(${PROJECT} glfw)

add_subdirectory(lib/vk-bootstrap)
target_link_libraries(${PROJECT} vk-bootstrap::vk-bootstrap)

add_subdirectory(lib/glm)
target_link_libraries(${PROJECT} glm::glm)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

option ( TRACY_ON_DEMAND " " ON )
add_compile_definitions(TRACY_VK_USE_SYMBOL_TABLE)
add_subdirectory(lib/tracy)
target_link_libraries(${PROJECT} Tracy::TracyClient)

add_subdirectory(lib/VulkanMemoryAllocator)
target_link_libraries(${PROJECT} GPUOpen::VulkanMemoryAllocator)

add_subdirectory(lib/result)
target_link_libraries(${PROJECT} Result::Result)
add_compile_definitions(RESULT_NAMESPACE=result)

add_definitions(-DGLFW_INCLUDE_NONE)

find_program(GLSLC glslc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
set(SHADER_SRC_DIRECTORY "${PROJECT_SOURCE_DIR}/engine/shaders")
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${SHADER_SRC_DIRECTORY}/*.glsl"
)

set(SPV_MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SPV_MAKE_DIRECTORY}")

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${SPV_MAKE_DIRECTORY}/${FILE_NAME}.spv")

    #if (MATCHES "\\.[^\.]*\\.glsl")
    string(REGEX MATCH "\\.[^\.]*\\.glsl" SHADER_STAGE ${GLSL})
    string(COMPARE NOTEQUAL "${SHADER_STAGE}" "" CONTAINS_STAGE)
    if (${CONTAINS_STAGE})
        string(REGEX REPLACE "\\.glsl" "" SHADER_STAGE ${SHADER_STAGE})
        string(REGEX REPLACE "\\." "" SHADER_STAGE ${SHADER_STAGE})

        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSLC} -g -I${SHADER_SRC_DIRECTORY} -fshader-stage=${SHADER_STAGE} ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
            COMMENT "Compiling ${GLSL} for stage: ${SHADER_STAGE}"
        )
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endif ()
endforeach(GLSL)

add_custom_target(
    shaders
    ALL
    DEPENDS ${SPIRV_BINARY_FILES}
)
