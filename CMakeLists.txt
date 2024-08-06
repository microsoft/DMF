cmake_minimum_required(VERSION 3.18)

# Query Windows SDK root directory.
# CMake populates these with a bunch of unnecessary libraries, which requires
# extra case-correcting symlinks and what not. Instead, let projects explicitly
# control which libraries they require.
file(
    WRITE "${CMAKE_BINARY_DIR}/MakeRulesOverwrite.cmake"
[=[
    if (MSVC)
        set(CMAKE_CXX_STANDARD_LIBRARIES_INIT " "  CACHE STRING "" FORCE)
        set(CMAKE_C_STANDARD_LIBRARIES_INIT " "  CACHE STRING "" FORCE)
    endif()
]=]
)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_BINARY_DIR}/MakeRulesOverwrite.cmake")

# Skip simpile Test program which may result in cmake failure
SET(CMAKE_C_COMPILER_WORKS 1)
SET(CMAKE_CXX_COMPILER_WORKS 1)
set_property(GLOBAL PROPERTY GLOBAL_DEPENDS_DEBUG_MODE 0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
message(FATAL_ERROR "In-tree builds are not supported. Run CMake from a separate directory: cmake -B build")
endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(DMF)
enable_language(CXX)
enable_language(C)
# enable_language(ASM_MARMASM)

# Variables
list(APPEND CMAKE_MODULE_PATH  "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

message("Detecting OS and Architecture")
include(DetectOS)

# validate supported OS
if(HOST_OS_LINUX)
    message(STATUS "OS: Linux")
elseif(HOST_OS_WINDOWS)
    message(STATUS "OS: Windows")
else()
    message(FATAL_ERROR "Unsupported OS")
    return()
endif()
	
# Validate supported architecture
if(TARGET_ARCH_X64)
	message(STATUS "Target Architecture: x64")
elseif(TARGET_ARCH_X86)
	message(STATUS "Target Architecture: x86")
elseif(TARGET_ARCH_ARM64)
	message(STATUS "Target Architecture: arm64")
else()
	message(FATAL_ERROR "Unsupported architecture")
	return()
endif()
	
if(HOST_OS_WINDOWS)
    message("Locating Windows Driver Kit")
    include(msvc-configurations)
    include(FindWDK)
elseif(HOST_OS_LINUX)
    message("Locating Linux Kernel Module Builder")
    include(LKM)
endif()

if(HOST_OS_WINDOWS) # msvc
	if( CMAKE_CXX_FLAGS_DEBUG MATCHES "/RTC1" )
	string( REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" )
	endif()	

	if( CMAKE_C_FLAGS_DEBUG MATCHES "/RTC1" )
	string( REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" )
	endif()

	if( CMAKE_CXX_FLAGS MATCHES "/EHsc" )
	string( REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
	endif()	

	if( CMAKE_C_FLAGS MATCHES "/EHsc" )
	string( REPLACE "/EHsc" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" )
	endif()

	if( CMAKE_CXX_FLAGS MATCHES "/GR" )
	string( REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
	endif()	

	if( CMAKE_C_FLAGS MATCHES "/GR" )
	string( REPLACE "/GR" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" )
	endif()
endif()

set(DMF_LIB_FINAL_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/$<CONFIG>/${TARGET_PLATFORM}/)

# DMF Lib list
set(DMF_KLIBS DmfK DmfKFramework DmfKModules.Library DmfKModules.Library.Tests DmfKModules.Template)
set(DMF_ULIBS DmfU DmfUFramework DmfUModules.Library )
foreach(DMF_LIB IN LISTS DMF_KLIBS)
	add_subdirectory(Dmf/Solution/${DMF_LIB})
endforeach()
foreach(DMF_LIB IN LISTS DMF_ULIBS)
	add_subdirectory(Dmf/Solution/${DMF_LIB})
endforeach()

#DMF Samples Codes
add_subdirectory(DmfSamples/InterfaceSample1)
add_subdirectory(DmfSamples/InterfaceSample2)

#Unit Test Code
add_subdirectory(DmfTest/DmfKTest/sys)
add_subdirectory(DmfTest/DmfUTest/sys)