# cgltf Upstream Provenance

- Project: https://github.com/jkuhlmann/cgltf
- Base version: `1.15`
- Base commit: `360db1a`
- Required overflow-fix PR: https://github.com/jkuhlmann/cgltf/pull/293
- Applied patch commit: `8211a9f12a729e7c2998bcc68f43c2ed2e5462d9`
- License: MIT, preserved in `LICENSE`.

`cgltf.h` is the upstream header from the applied patch commit. `cgltf.c` is
the project-owned private implementation translation unit; it only enables the
upstream single-header implementation and does not expose cgltf types publicly.
