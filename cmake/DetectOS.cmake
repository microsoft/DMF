#
# This script is used to detect the OS and architecture of the host system
#

# Detect host architecture
if(CMAKE_C_PLATFORM_ID STREQUAL "Windows")
    if(DEFINED ENV{PROCESSOR_ARCHITECTURE})
        if("$ENV{PROCESSOR_ARCHITECTURE}" MATCHES "AMD64" ) 
            set(HOST_ARCH  "x86")
        else()
            set(HOST_ARCH  "$ENV{PROCESSOR_ARCHITECTURE}")
        endif()
    else()
    # If no PROCESSOR_ARCHITECTURE found, set to x86 defaultly
        set(HOST_ARCH  "x86")
    endif()
    if(CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "x86")
        set(TARGET_ARCH_X86 TRUE BOOL)
        set(IS_CROSS_COMPILE FALSE BOOL)
        set(TARGET_PLATFORM "x86")
        elseif(CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
        set(TARGET_ARCH_X64 TRUE BOOL)
        set(TARGET_PLATFORM "x64")
        set(IS_CROSS_COMPILE FALSE BOOL)
        elseif(CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
        set(TARGET_ARCH_ARM64 TRUE BOOL)
        set(TARGET_PLATFORM "arm64")
        set(IS_CROSS_COMPILE TRUE BOOL)
    endif()
elseif(CMAKE_C_PLATFORM_ID STREQUAL "Linux")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(TARGET_ARCH_X86 TRUE BOOL)
        set(TARGET_PLATFORM "x86")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TARGET_ARCH_X64 TRUE BOOL)
        set(TARGET_PLATFORM "x64")
    endif()
endif()

message(STATUS "CMAKE_C_COMPILER_ARCHITECTURE_ID: ${CMAKE_C_COMPILER_ARCHITECTURE_ID}")
message(STATUS "HOST_ARCH: ${HOST_ARCH}")
message(STATUS "TARGET_PLATFORM: ${TARGET_PLATFORM}")
message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")

# Detect host operating system
string(REGEX MATCH "Linux" HOST_OS_LINUX ${CMAKE_SYSTEM_NAME})

if(WIN32)
    set(HOST_OS_WINDOWS TRUE BOOL)
endif()
