cmake_minimum_required(VERSION 3.20)
project(stoner_basis_encoder LANGUAGES C CXX)

option(STONER_ENCODER_DEBUG_NAMES "Retain Wasm debug names for host diagnosis" OFF)

add_executable(stoner_basis_encoder
    Source/stoner_basis_encoder.cpp
    Source/stoner_wasi_stubs.cpp
    ../ktx/external/basisu/encoder/basisu_backend.cpp
    ../ktx/external/basisu/encoder/basisu_basis_file.cpp
    ../ktx/external/basisu/encoder/basisu_bc7enc.cpp
    ../ktx/external/basisu/encoder/basisu_comp.cpp
    ../ktx/external/basisu/encoder/basisu_enc.cpp
    ../ktx/external/basisu/encoder/basisu_etc.cpp
    ../ktx/external/basisu/encoder/basisu_frontend.cpp
    ../ktx/external/basisu/encoder/basisu_gpu_texture.cpp
    ../ktx/external/basisu/encoder/basisu_kernels_sse.cpp
    ../ktx/external/basisu/encoder/basisu_opencl.cpp
    ../ktx/external/basisu/encoder/basisu_pvrtc1_4.cpp
    ../ktx/external/basisu/encoder/basisu_resample_filters.cpp
    ../ktx/external/basisu/encoder/basisu_resampler.cpp
    ../ktx/external/basisu/encoder/basisu_ssim.cpp
    ../ktx/external/basisu/encoder/basisu_uastc_enc.cpp
    ../ktx/external/basisu/transcoder/basisu_transcoder.cpp
)

target_compile_features(stoner_basis_encoder PRIVATE cxx_std_17)
target_compile_definitions(stoner_basis_encoder PRIVATE
    BASISD_SUPPORT_KTX2=1
    BASISD_SUPPORT_KTX2_ZSTD=0
    BASISU_SUPPORT_OPENCL=0
    BASISU_SUPPORT_SSE=0
    BASISU_DISABLE_KTX2_ALIGNMENT_WORKAROUND=1
    BASISU_STONER_SINGLE_THREAD=1
    BASISU_STONER_KTX_WRITER_ID="StonerGraphicsLab/022-v1"
    __EMSCRIPTEN__=1
    LIBKTX
)
target_include_directories(stoner_basis_encoder PRIVATE
    ../ktx/external/basisu
    ../ktx/external/basisu/encoder
    ../ktx/external/basisu/transcoder
)
target_compile_options(stoner_basis_encoder PRIVATE
    "-isystem${WASI_SDK_PREFIX}/share/wasi-sysroot/include/wasm32-wasi/noeh/c++/v1"
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -ffp-contract=off
    -fno-strict-aliasing
    -mno-reference-types
    -w
)
target_link_options(stoner_basis_encoder PRIVATE
    -nostartfiles
    -mno-reference-types
    -Wl,--no-entry
    -Wl,--gc-sections
    -Wl,--export=stoner_encoder_version
    -Wl,--export=stoner_alloc
    -Wl,--export=stoner_free
    -Wl,--export=stoner_cook
    -Wl,--export=stoner_result_ptr
    -Wl,--export=stoner_result_size
    -Wl,--export=stoner_release_result
    -Wl,--initial-memory=33554432
    -Wl,--max-memory=1073741824
    -Wl,-z,stack-size=2097152
)
if(STONER_ENCODER_DEBUG_NAMES)
    target_compile_options(stoner_basis_encoder PRIVATE -Oz -flto -g)
    target_link_options(stoner_basis_encoder PRIVATE
        -Oz
        -flto
        -g
        "-Wl,-Map=${CMAKE_BINARY_DIR}/stoner_basis_encoder.map"
    )
else()
    target_compile_options(stoner_basis_encoder PRIVATE -O3 -flto)
    target_link_options(stoner_basis_encoder PRIVATE -O3 -flto)
    target_link_options(stoner_basis_encoder PRIVATE -Wl,--strip-all)
endif()
target_link_libraries(stoner_basis_encoder PRIVATE
    "${WASI_SDK_PREFIX}/share/wasi-sysroot/lib/wasm32-wasi/crt1-reactor.o"
)
set_target_properties(stoner_basis_encoder PROPERTIES
    OUTPUT_NAME stoner_basis_encoder
    SUFFIX ".wasm"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
