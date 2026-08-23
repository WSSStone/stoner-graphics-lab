# NVIDIA FLIP Upstream Pin

- Repository: `https://github.com/NVlabs/flip`
- Revision: `b475eb4bf394ab877c42166c9eb0a84a02cc5b14`
- Upstream header: `src/cpp/FLIP.h`
- Upstream license: `LICENSE`
- Acquisition date: `2026-08-22`
- Integration: private CPU-only C++ single-header use; `FLIP_ENABLE_CUDA` is
  never defined and no Python FLIP package is used.

Inventory and exact digests are frozen in `SHA256SUMS`. This dependency is used
only by validation code and must never cross a runtime public-header or link
boundary.
