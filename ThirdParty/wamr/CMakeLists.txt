cmake_minimum_required(VERSION 3.20)
project(stoner_wamr LANGUAGES C ASM)

if(WIN32)
    set(WAMR_BUILD_PLATFORM "windows")
elseif(APPLE)
    set(WAMR_BUILD_PLATFORM "darwin")
else()
    set(WAMR_BUILD_PLATFORM "linux")
endif()

set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 0)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_FAST_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 0)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_LIB_PTHREAD 0)
set(WAMR_BUILD_LIB_WASI_THREADS 0)
set(WAMR_BUILD_SIMD 0)
set(WAMR_BUILD_REF_TYPES 1)
set(WAMR_BUILD_SHARED_MEMORY 0)
set(WAMR_BUILD_BULK_MEMORY 1)
set(WAMR_BUILD_MEMORY_PROFILING 0)
set(WAMR_BUILD_DUMP_CALL_STACK 1)
set(WAMR_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

include("${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake")

add_library(stoner_wamr STATIC ${WAMR_RUNTIME_LIB_SOURCE})
target_include_directories(stoner_wamr PUBLIC
    core/iwasm/include
)
if(MSVC)
    target_compile_options(stoner_wamr PRIVATE /w)
else()
    target_compile_options(stoner_wamr PRIVATE -w)
endif()
set_target_properties(stoner_wamr PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    POSITION_INDEPENDENT_CODE ON
)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(stoner_wamr PROPERTIES
        "ARCHIVE_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/lib"
    )
endforeach()
