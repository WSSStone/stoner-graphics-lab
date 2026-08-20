# SPIRV-Cross Provenance

- Upstream: https://github.com/KhronosGroup/SPIRV-Cross
- Commit: `a0fba56c34a6700f1724bf9b751da5b488a3775c`
- Commit date: 2026-01-22
- Version lineage: SPIRV-Cross 0.68.0 / Vulkan SDK 1.4.335
- License: Apache-2.0 OR MIT; the upstream `LICENSE` file is retained verbatim.
- Local modifications: none.

## Inventory

The private AssetCooker integration vendors the minimum upstream source graph
needed for SPIR-V parsing, reflection, GLSL-base behavior, and MSL generation:

- `spirv_cross.cpp`
- `spirv_cross_parsed_ir.cpp`
- `spirv_parser.cpp`
- `spirv_cfg.cpp`
- `spirv_glsl.cpp`
- `spirv_msl.cpp`
- `spirv_cross.hpp`
- `spirv.hpp`
- `spirv_cfg.hpp`
- `spirv_common.hpp`
- `spirv_cross_containers.hpp`
- `spirv_cross_error_handling.hpp`
- `spirv_cross_parsed_ir.hpp`
- `GLSL.std.450.h`
- `spirv_parser.hpp`
- `NonSemanticShaderDebugInfo100.h`
- `spirv_glsl.hpp`
- `spirv_msl.hpp`

`spirv_glsl.cpp` is required by the pinned upstream build graph because
`CompilerMSL` derives from `CompilerGLSL`. It does not enable a GLSL runtime
path. The library is linked only into `Tools/AssetCooker` and must not appear in
the Asset or runtime module dependency graph.

Verify the import from the repository root with:

```sh
shasum -a 256 -c ThirdParty/spirv-cross/SHA256SUMS
```
