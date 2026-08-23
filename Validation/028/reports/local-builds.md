# Feature 028 Local macOS Builds

## Host And Revision

- Host: Apple M4 Pro, macOS arm64
- Inherited revision: `66a20cc42881d3747d836f9f45257c37f7f3e039`
- Scope: the current uncommitted Feature 028 implementation worktree
- Environment: Conda environment `godot`, SCons 4.10.1

## Results

| Gate | Command | Result |
|---|---|---|
| Strict Debug | `conda run -n godot scons config=debug strict=1 -j8` | PASS |
| Strict Release | `conda run -n godot scons config=release strict=1 -j8` | PASS |
| Production demo regression | `Build/Mac/Debug/Tests/StonerTest --suite production-content-demo` | PASS |
| Architecture unit tests | `conda run -n godot python -m unittest Tests/test_verify_architecture.py` | PASS, 14 tests |
| Repository architecture scan | `conda run -n godot python Tests/verify_architecture.py` | PASS |

Both compiler configurations used the repository strict mode, which promotes
project warnings to errors. The linker emitted only the inherited macOS
deployment-target warning associated with the configured GLFW binary; no new
Feature 028 compiler warning or error was present.

The Debug and Release builds completed without leaving an active build process.
This report is local build evidence only; native rendering, sanitizer, and
cross-platform closeout evidence are recorded by their dedicated tasks.
