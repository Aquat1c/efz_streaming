cmake_minimum_required(VERSION 3.10)
project(efz_streaming_overlay)

# Set C++ standard to C++17 for std::filesystem support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force 32-bit build
if(MSVC)
    # Proper way to force 32-bit build
    set(CMAKE_GENERATOR_PLATFORM Win32)
    # Use appropriate architecture flags for 32-bit
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MACHINE:X86")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:X86")
    
    # Static linking of C++ Runtime - FIXED: Change /MD to /MT for static linking
    foreach(flag_var
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
    
    # Additional flags for compatibility with older systems
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /Gd")
    # Add more strict warning flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    # Disable specific warnings for external code
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100 /wd4505")
    # Optimize for older CPUs compatible with the 2005-era game
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /arch:SSE")
    
    # Disable annoying secure CRT warnings and codecvt deprecation warnings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

# Add these global definitions to prevent winsock.h inclusion
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-D_WINSOCKAPI_)
add_definitions(-DVERBOSE_LOGGING)

# Add console window support option
option(EFZ_ENABLE_CONSOLE "Enable debug console window" ON)
if(EFZ_ENABLE_CONSOLE)
    add_definitions(-DEFZ_ENABLE_CONSOLE)
endif()

# Define source files
set(SOURCES
    src/dllmain.cpp
    src/memory_reader.cpp
    src/game_data.cpp
    src/overlay_data.cpp
    src/logger.cpp
)

# Create a shared library (DLL) - This must come BEFORE setting target properties
add_library(efz_streaming_overlay SHARED ${SOURCES})

# IMPORTANT: Target-specific commands must come AFTER add_library
target_compile_definitions(efz_streaming_overlay PRIVATE 
    WIN32_LEAN_AND_MEAN
    _WINSOCKAPI_
    VERBOSE_LOGGING
)

# Include directories
target_include_directories(efz_streaming_overlay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/detours/include
)

# Link required libraries
target_link_libraries(efz_streaming_overlay PRIVATE 
    user32 
    kernel32
    psapi
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/detours/lib.X86/detours.lib
)

# Set output directories
set_target_properties(efz_streaming_overlay PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Set the runtime library - explicitly use static runtime
set_property(TARGET efz_streaming_overlay PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
