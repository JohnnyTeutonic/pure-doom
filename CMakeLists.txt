cmake_minimum_required(VERSION 3.10)
project(PureDoom VERSION 0.1.0 LANGUAGES CXX CUDA)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CUDA configuration
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Use a more reliable approach to separate CUDA and C++ compilation:
# Add a compiler definition to enable CUDA in our code
add_compile_definitions(ENABLE_CUDA)

# Important: Enable separable compilation for CUDA code
set(CMAKE_CUDA_SEPARABLE_COMPILATION ON)

# Set CUDA architectures (adjust according to your target GPUs)
if(${CMAKE_VERSION} VERSION_LESS "3.18.0")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_50,code=sm_50")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_60,code=sm_60")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_70,code=sm_70")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_75,code=sm_75")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_80,code=sm_80")
    # Add PTX code generation for runtime compilation support
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_80,code=compute_80")
else()
    set(CMAKE_CUDA_ARCHITECTURES 50 60 70 75 80)
endif()

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

# Find CUDA libraries (required for GPU acceleration)
find_package(CUDA REQUIRED)
include_directories(${CUDA_INCLUDE_DIRS})

# CUDA specific settings
if(CMAKE_CUDA_COMPILER)
    # Set CUDA flags
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -std=c++14")
    
    # Enable device code linking with proper flags to fix the linking error
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --device-link")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --relocatable-device-code=true")

    # Add flag to allow host code in device code
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --extended-lambda")
    
    # Add verbose PTX generation for debugging
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --ptxas-options=-v")
    
    # Add debug info in debug mode
    set(CMAKE_CUDA_FLAGS_DEBUG "${CMAKE_CUDA_FLAGS_DEBUG} -G")
    
    # Print CUDA info
    message(STATUS "CUDA compiler: ${CMAKE_CUDA_COMPILER}")
    message(STATUS "CUDA flags: ${CMAKE_CUDA_FLAGS}")
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.18.0")
        message(STATUS "CUDA architectures: ${CMAKE_CUDA_ARCHITECTURES}")
    endif()
else()
    message(WARNING "CUDA compiler not found. GPU acceleration will be disabled.")
endif()

# Create separate targets for CUDA and C++ to avoid conflicts
set(CUDA_SOURCES
    src/CudaUtils.cu
    src/RendererCuda.cu
    src/CudaTestMap.cu  # Added CudaTestMap source
)

# Common C++ source files without main.cpp
set(COMMON_CPP_SOURCES
    src/BSPTree.cpp
    src/Renderer.cpp
    src/Sprite.cpp
    src/TextureLoader.cpp
)

# Create CUDA test map executable sources
set(TEST_MAP_SOURCES
    src/TestMap.cpp
)

# Main executable source
set(MAIN_SOURCE
    src/main.cpp
)

# Add object library for CUDA files
add_library(cuda_objects OBJECT ${CUDA_SOURCES})
set_target_properties(cuda_objects PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(cuda_objects PRIVATE src ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

# Add object library for common C++ files (excluding main.cpp)
add_library(common_cpp_objects OBJECT ${COMMON_CPP_SOURCES})
set_target_properties(common_cpp_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(common_cpp_objects PRIVATE src)

# Add executable
add_executable(PureDoom ${MAIN_SOURCE} $<TARGET_OBJECTS:cuda_objects> $<TARGET_OBJECTS:common_cpp_objects>)

# Add CUDA test map executable - use shared objects but not main.cpp
add_executable(cuda_test_map ${TEST_MAP_SOURCES} $<TARGET_OBJECTS:cuda_objects> $<TARGET_OBJECTS:common_cpp_objects>)
set_target_properties(cuda_test_map PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
target_include_directories(cuda_test_map PRIVATE src ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

# Include directories
target_include_directories(PureDoom PRIVATE src ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

# Link libraries
target_link_libraries(PureDoom 
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${CUDA_LIBRARIES}
    ${CUDA_CUDA_LIBRARY}
    ${CUDA_CUDART_LIBRARY}
    ${CUDA_curand_LIBRARY}
)

# Link libraries for the test map executable
target_link_libraries(cuda_test_map
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${CUDA_LIBRARIES}
    ${CUDA_CUDA_LIBRARY}
    ${CUDA_CUDART_LIBRARY}
    ${CUDA_curand_LIBRARY}
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

# Set output directory for test map
set_target_properties(cuda_test_map
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Optional: Define a fallback macro if CUDA is not available
if(NOT CMAKE_CUDA_COMPILER)
    target_compile_definitions(PureDoom PRIVATE NO_CUDA_AVAILABLE)
endif()

# Print status message
message(STATUS "Configuring PureDoom with BSP Tree, Renderer, Sprite support, texture loading, and CUDA acceleration")
message(STATUS "SDL2_IMAGE libraries: ${SDL2_IMAGE_LIBRARIES}")
if(CMAKE_CUDA_COMPILER)
    message(STATUS "CUDA libraries: ${CUDA_LIBRARIES}")
endif() 