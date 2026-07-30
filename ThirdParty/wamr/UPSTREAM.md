# WAMR Provenance

- Project: bytecodealliance/wasm-micro-runtime
- Release tag: `WAMR-2.4.5`
- Source commit: `25bd7eb63e828e4bd242cc9b38d260b4b31c6605`
- License: Apache-2.0 with LLVM exception
- Upstream: https://github.com/bytecodealliance/wasm-micro-runtime

The repository-owned CMake wrapper builds only the classic/fast interpreter
runtime with bulk-memory and reference-types parsing required by wasi-sdk 33
output. AOT, JIT, WASI, builtin libc, sockets, threads, shared memory, SIMD,
multi-module loading, and native extension registration are disabled for the
Feature 022 encoder host.

The vendored version-header configure step fixes its newline style to LF so
Windows builds do not mutate the pinned source snapshot before provenance
verification.
