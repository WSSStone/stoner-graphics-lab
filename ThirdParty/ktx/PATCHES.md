# Local Integration Patches

`external/basisu/encoder/basisu_comp.cpp` accepts the compile-time
`BASISU_STONER_KTX_WRITER_ID` override. Native libktx builds do not define it
and retain upstream behavior. The authoritative encoder module defines it as
`StonerGraphicsLab/022-v1`, avoiding host rewriting of compressed KTX2 bytes.

The same file has a `BASISU_STONER_SINGLE_THREAD`-guarded DFD correction for
the authoritative module: ETC1S BasisLZ output writes the nonzero
`bytesPlane0` and optional alpha `bytesPlane1` values required since KTX
specification 2.0.4. Native libktx and non-Stoner Basis builds retain upstream
behavior.
