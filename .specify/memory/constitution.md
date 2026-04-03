<!--
Sync Impact Report:
- Version change: 1.0.0 -> 1.1.0
- Modified principles: None
- Added sections:
  - VI. Naming Conventions (UE5 Style)
- Removed sections: None
- Templates requiring updates:
  - ✅ .specify/templates/plan-template.md
  - ✅ .specify/templates/spec-template.md
  - ✅ .specify/templates/tasks-template.md
- Follow-up TODOs: None
-->
# Stoner Graphics System Constitution

## Core Principles

### I. Spec-Driven Development (SSD)
Strict adherence to "design before coding". All major features and refactoring MUST begin with updating or creating specification documents and the top-level constitution.

### II. Decoupled Architecture (RHI Abstraction)
The system MUST maintain a strict `Application <-> RHI (Render Hardware Interface) <-> Graphics API` layered architecture. The Application layer MUST NEVER directly call specific Graphics API functions.

### III. Design Pattern Discipline
God-classes and giant functions are strictly prohibited. The system MUST heavily utilize the Strategy Pattern and Composite Pattern to decouple orthogonal responsibilities such as rule distribution, spatial constraints, and geometric placement.

### IV. Multi-API Support
The RHI MUST be designed to support modern explicit APIs (Vulkan, DX12, Metal) as first-class citizens, while maintaining compatibility paths for legacy/web APIs (DX11, OpenGL, GLES, WebGL).

### V. Advanced Graphics Readiness
The core architecture MUST anticipate and support advanced rendering techniques including hardware/software Ray Tracing, Nanite-like meshlet optimization, and Lumen-like global illumination. Data structures and pipelines MUST be designed with these paradigms in mind.

### VI. Naming Conventions (UE5 Style)
The project MUST adopt PascalCase, UnrealEngine5-style naming conventions for all C++ code. This includes using appropriate prefixes (e.g., `U` for objects, `A` for actors, `F` for structs/vectors, `E` for enums, `I` for interfaces, `T` for templates) to ensure consistency and readability across the codebase.

## Technology Stack & Standards

- **Primary Language**: C++ (Modern C++, e.g., C++20/23).
- **Graphics APIs**: Vulkan, DirectX 12, DirectX 11, Metal, OpenGL, OpenGL ES, WebGL.
- **Version Control**: Managed via `ugit`.

## Development Workflow

- **Design First**: Specifications MUST be written and approved before implementation begins.
- **Commit Protocol**: Since `ugit` is used, agents MUST provide recommended commit messages at the end of their output.

## Governance

- This Constitution supersedes all other practices.
- Amendments require documentation, approval, and a migration plan.
- All PRs/reviews MUST verify compliance with the RHI abstraction and design pattern disciplines.

**Version**: 1.1.0 | **Ratified**: 2026-04-03 | **Last Amended**: 2026-04-03
