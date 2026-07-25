# Execution

Execute one `crctl next` packet per session.

## B00: Bootstrap
- B00-S01: Framework and Draft PR
- B00-S02: Baseline and CodeGraph

## B01: Build, CI, and Architecture
- B01-S01: Inspect
- B01-S02: Fix
- B01-S03: Verify

## B02: Core Features 003-006
- B02-S01: Inspect
- B02-S02: Fix
- B02-S03: Verify

## B03: RHI Features 007-008
- B03-S01: Inspect
- B03-S02: Fix
- B03-S03: Verify

## B04: Vulkan Foundation 009-010
- B04-S01: Inspect
- B04-S02: Fix
- B04-S03: Verify

## B05: Vulkan Execution 011-012
- B05-S01: Inspect
- B05-S02: Fix
- B05-S03: Verify

## B06: Renderer 013-015
- B06-S01: Inspect
- B06-S02: Fix
- B06-S03: Verify

## B07: Application 016-017
- B07-S01: Inspect
- B07-S02: Fix
- B07-S03: Verify

## B08: Integration 018-019
- B08-S01: Inspect
- B08-S02: Fix
- B08-S03: Verify

## B09: Cross-Cutting
- B09-S01: Inspect
- B09-S02: Fix
- B09-S03: Verify

## B10: Closeout
- B10-S01: Traceability
- B10-S02: Final Gates
- B10-S03: Close

## Required Gates

- Debug and Release
- ASan/UBSan after B01 introduces the profiles
- Deterministic and required native validation
- Three-platform GitHub CI at batch boundaries
- Final CodeGraph rebuild and coverage report
