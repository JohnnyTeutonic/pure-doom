cmake_minimum_required(VERSION 3.10)
project(PureDoom VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2 package
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Add executable
add_executable(PureDoom
    src/main.cpp
    src/BSPTree.cpp
    src/Renderer.cpp
    src/Sprite.cpp
)

# Include directories
target_include_directories(PureDoom PRIVATE src)

# Link SDL2
target_link_libraries(PureDoom ${SDL2_LIBRARIES})

# Set compiler warnings
if(MSVC)
    target_compile_options(PureDoom PRIVATE /W4)
else()
    target_compile_options(PureDoom PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set output directory
set_target_properties(PureDoom
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Print status message
message(STATUS "Configuring PureDoom with BSP Tree, Renderer, and Sprite support") 