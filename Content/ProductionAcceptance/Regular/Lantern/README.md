# Lantern Regular Fixture

`Lantern.glb` is the bounded, checked-in production-content root used by the
regular Feature 028 gate. Its authoritative revision, size, and SHA-256 are
recorded in `../../UPSTREAM.md` and the canonical corpus manifest.

The package contains one GLB file with embedded buffers and images. Validation
must access it through the ordinary resolver and glTF importer; fixture-specific
parsing or semantic oracles are forbidden.
