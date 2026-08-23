# Production Corpus Upstream Pins

## Khronos glTF Sample Assets

- Repository: `https://github.com/KhronosGroup/glTF-Sample-Assets`
- Revision: `bf2bb4a81c73a7ceb53e80df3dec0105c5a3fdef`
- Acquisition date: `2026-08-22`

### Regular Lantern Package

- Upstream path: `Models/Lantern/glTF-Binary/Lantern.glb`
- Checked-in path: `Regular/Lantern/Lantern.glb`
- Inventory: one regular file, 9,564,264 bytes
- SHA-256: `a79458c4b02d695187a952f23a63b8bf278e7bc3d316a3c2a314f2d6974181f1`

### External Medium Sponza Package

- Upstream root: `Models/Sponza/glTF/`
- Staging root: `External/Sponza/`
- Root asset: `Sponza.gltf`
- Inventory: 71 regular files, 52,686,624 aggregate bytes
- First canonical path: `10381718147657362067.jpg`
- Last canonical path: `white.png`
- Pinned Git-tree research digest:
  `d569a207a034cee944a05907fa3e748d75b9313610d4656f3bd0b42505ceed6d`

The research digest is SHA-256 over UTF-8 lines sorted by upstream Git tree
order. Each line is `path<TAB>size<TAB>git-blob-sha<LF>`. Runtime acquisition
uses the canonical corpus manifest's SHA-256 inventory instead of Git blob IDs.

## Policy Boundary

Automated acceptance consumes only technical source revision, path, inventory,
size, and digest fields. It does not read license metadata or make licensing or
policy decisions. `MAINTAINER_NOTES.md` remains outside every package root and
validator input.
