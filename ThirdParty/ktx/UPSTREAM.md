# KTX-Software Provenance

- Project: KhronosGroup/KTX-Software
- Release tag: `v4.4.2`
- Source commit: `4d6fc70eaf62ad0558e63e8d97eb9766118327a6`
- License: Apache-2.0; bundled dependency licenses are under `LICENSES/`
- Upstream: https://github.com/KhronosGroup/KTX-Software

The repository vendors the memory-oriented libktx, DFD, ETC, Basis encoder,
Basis transcoder, and zstd source files required by Feature 022. OpenGL and
Vulkan upload helpers, command-line tools, image decoders, examples, tests, and
documentation assets are intentionally excluded.

`CMakeLists.txt` is a repository-owned wrapper. It builds one private static
library with upstream `KTX_FEATURE_WRITE` enabled, OpenCL and host SIMD
disabled, no KTX2 zstd transcode dependency, and no graphics-loader entry
points. A guarded writer patch preserves an explicit repository-owned
`KTXwriter` value instead of appending the libktx build version; upstream
behavior remains active when `STONER_KTX_FIXED_WRITER` is not defined.
