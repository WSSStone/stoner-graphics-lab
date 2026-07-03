<!--
Sync Impact Report:
- Version change: 1.2.0 -> 1.3.0
- Modified principles:
  - VII. Cross-Platform Compatibility (expanded with automated validation requirement)
- Added sections:
  - None
- Added to Technology Stack:
  - None
- Removed sections: None
- Templates requiring updates:
  - ✅ .specify/templates/plan-template.md (Constitution Check updated for automated cross-platform validation)
  - ✅ .specify/templates/spec-template.md (Architecture Constraints updated for automated cross-platform validation)
  - ✅ .specify/templates/tasks-template.md (Polish phase updated for CI/equivalent validation)
  - N/A .specify/templates/commands/*.md (directory absent in this project)
  - ✅ doc/roadmap.md (Constitution version references updated)
- Follow-up TODOs: None
-->
# Stoner Graphics System Constitution

## Core Principles

### I. Spec-Driven Development (SSD)
Strict adherence to "design before coding". All major features and refactoring
MUST begin with updating or creating specification documents and the top-level
constitution.

### II. Decoupled Architecture (RHI Abstraction)
The system MUST maintain a strict
`Application <-> RHI (Render Hardware Interface) <-> Graphics API` layered
architecture. The Application layer MUST NEVER directly call specific Graphics
API functions.

### III. Design Pattern Discipline
God-classes and giant functions are strictly prohibited. The system MUST heavily
utilize the Strategy Pattern and Composite Pattern to decouple orthogonal
responsibilities such as rule distribution, spatial constraints, and geometric
placement.

### IV. Multi-API Support
The RHI MUST be designed to support modern explicit APIs (Vulkan, DX12, Metal)
as first-class citizens, while maintaining compatibility paths for legacy/web
APIs (DX11, OpenGL, GLES, WebGL).

### V. Advanced Graphics Readiness
The core architecture MUST anticipate and support advanced rendering techniques
including hardware/software Ray Tracing, Nanite-like meshlet optimization, and
Lumen-like global illumination. Data structures and pipelines MUST be designed
with these paradigms in mind.

### VI. Naming Conventions (UE5 Style)
The project MUST adopt PascalCase, UnrealEngine5-style naming conventions for
all C++ code. This includes using appropriate prefixes (e.g., `U` for objects,
`A` for actors, `F` for structs/vectors, `E` for enums, `I` for interfaces,
`T` for templates) to ensure consistency and readability across the codebase.

### VII. Cross-Platform Compatibility
All source code, build scripts, and runtime logic MUST support development,
building, and execution across multiple platforms (Windows, macOS, Linux at
minimum). Platform-specific code MUST be isolated behind abstraction layers or
conditional compilation guards. Build configurations MUST NOT assume a single
OS, shell, or toolchain. Developers MUST verify that new features compile and
run correctly on all supported platforms before merging.

Features that affect build scripts, platform abstractions, runtime startup,
windowing, input, rendering backends, public cross-platform APIs, or other
platform-sensitive behavior MUST include or update an automated cross-platform
validation path covering Windows, macOS, and Linux. GitHub Actions is the
default hosted CI mechanism for this project unless an equivalent documented CI
system is used. If automated coverage is temporarily unavailable for a supported
platform, the feature plan MUST document the gap, the fallback manual
verification command, and a follow-up task before implementation is considered
complete.

## Technology Stack & Standards

- **Primary Language**: C++ (Modern C++, e.g., C++20/23).
- **Build System**: SCons 4.10.1.
- **Graphics APIs**: Vulkan, DirectX 12, DirectX 11, Metal, OpenGL,
  OpenGL ES, WebGL.
- **Version Control**: Managed via `ugit`.

## Development Workflow

- **Design First**: Specifications MUST be written and approved before
  implementation begins.
- **Automated Validation**: Platform-sensitive features MUST keep the
  cross-platform CI or equivalent validation path current with their build and
  headless test requirements.
- **Commit Protocol**: Since `ugit` is used, agents MUST provide recommended
  commit messages at the end of their output.

## Governance

- This Constitution supersedes all other practices.
- Amendments require documentation, approval, and a migration plan.
- All PRs/reviews MUST verify compliance with the RHI abstraction, design
  pattern disciplines, and cross-platform compatibility.

**Version**: 1.3.0 | **Ratified**: 2026-04-03 | **Last Amended**: 2026-07-03
