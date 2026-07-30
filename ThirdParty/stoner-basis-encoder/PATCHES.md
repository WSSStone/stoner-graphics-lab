# Encoder Module Patches

The authoritative module compiles the Basis Universal sources from the pinned
KTX-Software 4.4.2 tree. Module-only changes are guarded by
`BASISU_STONER_SINGLE_THREAD`:

- replace mutexes, condition variables, and worker jobs with synchronous
  equivalents;
- suppress logging, timing, and alignment warning output;
- emit nonzero ETC1S `bytesPlane0` and alpha `bytesPlane1` values required by
  KTX specification 2.0.4 instead of the pinned encoder's deprecated unsized
  DFD form;
- use standards-defined `mipPadding` instead of inserting Basis Universal's
  legacy all-`0x7f` dummy KVD key for UASTC level alignment;
- skip host CPU feature detection, OpenCL, and transcoder-global startup not
  needed by the encoder;
- initialize immutable ETC/BC7/ASTC lookup tables from
  `Source/stoner_basis_tables.h` instead of recomputing them in the
  interpreter;
- fix the KTX writer identifier to `StonerGraphicsLab/022-v1`.

Native libktx and Basis transcode builds do not define the guard and retain
upstream behavior. The WebAssembly wrapper, capability-denial stubs, canonical
request ABI, and generated table header are repository-owned Feature 022
sources.
