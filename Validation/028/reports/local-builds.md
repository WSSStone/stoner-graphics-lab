# Feature 028 Local macOS Builds

## Host And Revision

- Host: Apple M4 Pro, macOS arm64
- Revision: `0a5ad11f8d511a9b54da33da086b15cf530ca68a`
- Scope: final Sponza v2 camera, normal, image acceptance, and lifecycle-gate code
- Environment: repository bundled Python/SCons 4.10.1 toolchain

## Results

| Gate | Command | Result |
|---|---|---|
| Strict Debug | `conda run -n godot scons config=debug strict=1 -j8` | PASS |
| Strict Release | `conda run -n godot scons config=release strict=1 -j8` | PASS |
| Production demo regression | `Build/Mac/Debug/Tests/StonerTest --suite production-content-demo` | PASS |
| Production image acceptance | `Build/Mac/Release/Tests/StonerTest --suite production-image-acceptance` | PASS, 19 checks |
| Camera preview regression | `Build/Mac/Debug/Tests/StonerTest --suite production-camera-preview` | PASS, 15 checks |
| Architecture unit tests | `conda run -n godot python -m unittest Tests/test_verify_architecture.py` | PASS, 14 tests |
| Repository architecture scan | `conda run -n godot python Tests/verify_architecture.py` | PASS |

Both compiler configurations used the repository strict mode, which promotes
project warnings to errors. The linker emitted only the inherited macOS
deployment-target warning associated with the configured GLFW binary; no new
Feature 028 compiler warning or error was present.

The Debug and Release builds completed without leaving an active build process.
This report is local build evidence only; native rendering, sanitizer, and
cross-platform closeout evidence are recorded by their dedicated tasks.
