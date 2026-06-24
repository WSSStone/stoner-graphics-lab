<!-- SPECKIT START -->
For additional context about technologies to be used, project structure,
shell commands, and other important information, read the current plan:
`specs/007-core-platform-abstraction/plan.md`
<!-- SPECKIT END -->

## Active Technologies
- C++20 (traditional header/source separation; no C++20 Modules) + C++ standard library where portable (`<chrono>`, `<filesystem>`, `<fstream>`, `<system_error>`, `<thread>`); platform system libraries guarded behind Core implementation boundaries; SCons 4.10.1 build system (007-core-platform-abstraction)
- Local filesystem only for basic read/write/existence/directory operations; no persistent database or asset catalog (007-core-platform-abstraction)

## Recent Changes
- 007-core-platform-abstraction: Added C++20 (traditional header/source separation; no C++20 Modules) + C++ standard library where portable (`<chrono>`, `<filesystem>`, `<fstream>`, `<system_error>`, `<thread>`); platform system libraries guarded behind Core implementation boundaries; SCons 4.10.1 build system
