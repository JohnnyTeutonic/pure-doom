cmake_minimum_required(VERSION 3.10)
project(PureDoom VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2 package
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Set up SDL2_image (the correct way)
# Attempt to use pkg-config first
include(FindPkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2_IMAGE SDL2_image)
endif()

# If pkg-config didn't work, try manual approach
if(NOT SDL2_IMAGE_FOUND)
    find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h
        PATH_SUFFIXES SDL2
        PATHS ${SDL2_DIR}
    )

    find_library(SDL2_IMAGE_LIBRARY
        NAMES SDL2_image
        PATHS ${SDL2_DIR}
    )

    if(SDL2_IMAGE_INCLUDE_DIR AND SDL2_IMAGE_LIBRARY)
        set(SDL2_IMAGE_FOUND TRUE)
        set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})
        set(SDL2_IMAGE_LIBRARIES ${SDL2_IMAGE_LIBRARY})
    endif()
endif()

if(NOT SDL2_IMAGE_FOUND)
    message(FATAL_ERROR "SDL2_image library not found. Please install it.")
endif()

include_directories(${SDL2_IMAGE_INCLUDE_DIRS})

# Add executable
add_executable(PureDoom
    src/main.cpp
    src/BSPTree.cpp
    src/Renderer.cpp
    src/Sprite.cpp
    src/TextureLoader.cpp
)

# Include directories
target_include_directories(PureDoom PRIVATE src)

# Link libraries
target_link_libraries(PureDoom 
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
)

# Set compiler warnings
if(MSVC)
    target_compile_options(PureDoom PRIVATE /W4)
else()
    target_compile_options(PureDoom PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Create textures directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/textures)

# Set output directory
set_target_properties(PureDoom
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Print status message
message(STATUS "Configuring PureDoom with BSP Tree, Renderer, Sprite support, and texture loading")
message(STATUS "SDL2_IMAGE libraries: ${SDL2_IMAGE_LIBRARIES}") 