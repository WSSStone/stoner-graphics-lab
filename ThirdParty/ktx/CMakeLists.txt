cmake_minimum_required(VERSION 3.20)
project(stoner_ktx LANGUAGES C CXX)

add_library(stoner_ktx STATIC
    external/basisu/encoder/basisu_backend.cpp
    external/basisu/encoder/basisu_basis_file.cpp
    external/basisu/encoder/basisu_bc7enc.cpp
    external/basisu/encoder/basisu_comp.cpp
    external/basisu/encoder/basisu_enc.cpp
    external/basisu/encoder/basisu_etc.cpp
    external/basisu/encoder/basisu_frontend.cpp
    external/basisu/encoder/basisu_gpu_texture.cpp
    external/basisu/encoder/basisu_kernels_sse.cpp
    external/basisu/encoder/basisu_opencl.cpp
    external/basisu/encoder/basisu_pvrtc1_4.cpp
    external/basisu/encoder/basisu_resample_filters.cpp
    external/basisu/encoder/basisu_resampler.cpp
    external/basisu/encoder/basisu_ssim.cpp
    external/basisu/encoder/basisu_uastc_enc.cpp
    external/basisu/transcoder/basisu_transcoder.cpp
    external/basisu/zstd/zstd.c
    external/dfdutils/colourspaces.c
    external/dfdutils/createdfd.c
    external/dfdutils/interpretdfd.c
    external/dfdutils/printdfd.c
    external/dfdutils/queries.c
    external/dfdutils/vk2dfd.c
    external/etcdec/etcdec.cxx
    lib/basis_encode.cpp
    lib/basis_transcode.cpp
    lib/checkheader.c
    lib/etcunpack.cxx
    lib/filestream.c
    lib/formatsize.h
    lib/glformat_str.c
    lib/hashlist.c
    lib/info.c
    lib/memstream.c
    lib/miniz_wrapper.cpp
    lib/strings.c
    lib/swap.c
    lib/texture.c
    lib/texture1.c
    lib/texture2.c
    lib/vkformat_check.c
    lib/vkformat_check_variant.c
    lib/vkformat_str.c
    lib/vkformat_typesize.c
    lib/writer1.c
    lib/writer2.c
)

target_compile_features(stoner_ktx PRIVATE cxx_std_17)
target_compile_definitions(stoner_ktx PRIVATE
    BASISD_SUPPORT_KTX2=1
    BASISD_SUPPORT_KTX2_ZSTD=0
    BASISU_SUPPORT_OPENCL=0
    BASISU_SUPPORT_SSE=0
    KTX_FEATURE_WRITE=1
    KHRONOS_STATIC
    LIBKTX
    STONER_KTX_FIXED_WRITER=1
)
target_include_directories(stoner_ktx
    PUBLIC
        include
    PRIVATE
        external
        external/basisu
        external/basisu/encoder
        external/basisu/transcoder
        external/basisu/zstd
        external/dfdutils
        external/dfdutils/vulkan
        lib
        other_include
        utils
)

if(MSVC)
    target_compile_options(stoner_ktx PRIVATE /w)
else()
    target_compile_options(stoner_ktx PRIVATE
        -w
        -fno-strict-aliasing
        -ffp-contract=off
    )
endif()

set_target_properties(stoner_ktx PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    POSITION_INDEPENDENT_CODE ON
)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(stoner_ktx PROPERTIES
        "ARCHIVE_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/lib"
    )
endforeach()
