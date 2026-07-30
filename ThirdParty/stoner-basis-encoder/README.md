# Stoner Basis Encoder

This directory owns the authoritative Feature 022 encoder WebAssembly module.
It is built from the Basis Universal sources vendored with KTX-Software 4.4.2
using wasi-sdk 33.0 (`clang 22.1.0`) and a repository-owned memory-only ABI.
The checked-in module SHA-256 is
`d394459dc8f85d2e133045c421b61ef6e080f5890c77f7f048a7258ee77e0b98`.

The canonical request is little-endian:

1. 32-byte header: magic `SGK2`, ABI version, policy (`1=ETC1S`,
   `2=UASTC`), quality (`0=Balanced`, `1=High`), flags (`sRGB`, normal,
   force-alpha), mip count, key/value count, reserved;
2. one 16-byte descriptor per mip: width, height, byte offset, byte length;
3. one 16-byte descriptor per metadata item: key offset/length and value
   offset/length;
4. NUL-terminated UTF-8 keys, values, and tightly packed RGBA8 mip bytes.

The module exports:

- `stoner_encoder_version`
- `stoner_alloc` / `stoner_free`
- `stoner_cook`
- `stoner_result_ptr` / `stoner_result_size`
- `stoner_release_result`

It imports no project API. Each WAMR instance and its linear memory belong to
one cook request. The module fixes one worker, disables SIMD/OpenCL/UASTC RDO,
and writes the complete compressed KTX2 container. The host validates and
hashes that byte range but never rewrites it.

The module also links local capability-denial stubs for libc's otherwise
unreachable WASI calls, so the final binary has no host imports. The stubs
discard output, deny filesystem access, trap on process exit, and return a
fixed byte pattern for randomness. Canonical encoder code does not consume
those services.

Basis normally generates several immutable ETC/BC7/ASTC lookup tables at
startup. Interpreter execution of those nested searches is needlessly slow, so
`Source/stoner_basis_tables.h` records the exact bytes produced by the pinned
native integer-only initialization code. `PATCHES.md` records the narrow
module-only substitutions. They change startup work, not table contents.

Rebuild:

```bash
cmake -S ThirdParty/stoner-basis-encoder \
  -B Build/EncoderWasm \
  -DCMAKE_TOOLCHAIN_FILE="$WASI_SDK_PATH/share/cmake/wasi-sdk.cmake" \
  -DWASI_SDK_PREFIX="$WASI_SDK_PATH" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build Build/EncoderWasm --config Release
shasum -a 256 Build/EncoderWasm/bin/stoner_basis_encoder.wasm
```

The expected release hash is the value above. Build with
`-DSTONER_ENCODER_DEBUG_NAMES=ON` to retain function names and emit a linker
map for diagnosis; debug output is not the checked-in module.
