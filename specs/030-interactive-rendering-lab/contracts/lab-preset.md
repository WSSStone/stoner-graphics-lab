# Interactive Lab Preset v1

## Authority and schema

This is a new, non-authoritative user export, separate from the historical `stoner.production-camera-candidate` format. Formal scene validation never reads it. Runtime import accepts one UTF-8 JSON object, <=64 KiB, maximum depth 8, <=256 values, <=4096 bytes per identity string. Reject duplicate/unknown keys, invalid UTF-8, embedded NUL, non-finite or overflowing numbers, trailing data and unsupported versions before state changes. No paths, executable commands, native handles or GPU resources are stored in the file.

| Field | Type and meaning |
| --- | --- |
| `schema` / `schemaVersion` | Literal `stoner.interactive-lab-preset` / integer 1 |
| `authority` | Literal `interactive-preview` |
| `workload` | `revision`, `productionRoot`, `sourceIdentityDigest` |
| `sourceContext` | Exported `drawableWidth`, `drawableHeight`, `backend`, `cookedGeneration`, `softwareRevision` |
| `camera` | `position[3]`, `yawRadians`, `pitchRadians`, `verticalFovRadians`, `nearPlane`, `farPlane`, `movementSpeed` |
| `output` | `requestedProfileId`, `sdrToneMapVersion`, `hdrViewingVersion`, `exposureStops`, structured `debugBypass` object defined below |
| `ui` | `whiteMultiplier`; visibility remains a session control and is not restored by preset import |
| `digest` | Lowercase SHA-256 of the normalized pinned-writer object excluding this field |

All fields are required. Camera uses the existing yaw/pitch no-roll free-camera convention. No serialized derived projection matrix may override the current drawable aspect. Numeric bounds match [camera-input.md](camera-input.md) and [lab-runtime.md](lab-runtime.md); exported extents must be non-zero and within drawable limits. Output version identities must be in Feature 029's frozen registry; debug selection must resolve against that profile's existing stage registry, not an arbitrary shader name.

Workload compatibility is exact revision, productionRoot and sourceIdentityDigest equality. The digest represents a canonical sorted source-identity/version closure of the selected production root, independent of backend-target cooking. Target/backend/cooked-generation fields are provenance only: valid equivalent target packages may differ. Unknown or mismatching content identities fail rather than guess from a file name. Different current drawable extents alone never invalidate a preset.

## Debug selection wire contract

`output.debugBypass` is a required object with exactly these required fields:

| Field | Value and validation |
| --- | --- |
| `mode` | Existing mode identifier: `Disabled`, `BoundedVisualization` or `HDRPreservingReadback` |
| `stageName` | Stable existing stage name, validated against the selected workload/output graph; bounded by the identity-string limit |
| `sourceDomain` | Existing Renderer color-domain identifier resolved for that stage and requested profile; serialized as a checkable assertion, never as an instruction to reinterpret the texture |
| `visualizationMinimum` / `visualizationMaximum` | Finite float32 bounds; minimum < maximum and their float32 difference must be finite and positive; defaults 0/1 |

For `Disabled`, `stageName` is empty and `sourceDomain` is `Unspecified`; the finite visualization range still round-trips. For either enabled mode, require a known non-empty stage and an exact match between recorded domain and freshly resolved stage domain; reject the whole import on mismatch or unavailable stage. Never serialize process-local stage IDs. Profile/stage edits resolve a new coherent domain before export; preset import verifies the recorded domain rather than silently changing it.

The panel's `NumericReadback` selection maps to the existing `HDRPreservingReadback` mode. It selects a source only: restoring or selecting this mode does not enqueue a capture or create readback resources. A separate explicit capture action is required. `BoundedVisualization` remains a GPU-only widget. All five fields participate in encoding/digest and exact float32 round-trip tests, including non-default ranges and enabled stages.

## Encoding, digest and round-trip

Use the already pinned yyjson 0.12.0 parser and writer through the Application-private codec. After validating a complete typed record, construct a mutable object in the field order shown in the schema tables, including each nested object's listed order, and write with `YYJSON_WRITE_NOFLAG` (compact UTF-8 JSON). Insert bounded integers as integers and each finite float32 setting as its exact double promotion; normalize negative zero to positive zero. Use the writer's existing numeric and string formatting. No custom float formatter, forced exponent syntax, recursive key sorter or new canonical-JSON library is required.

`digest` is lowercase SHA-256 over that writer output with the digest field omitted and no trailing newline. Add the digest last when exporting. Import accepts ordinary valid whitespace and key ordering, reconstructs the validated typed record in schema order, and uses the same pinned writer to verify the digest. Unknown/duplicate fields are rejected before reconstruction. A dependency update that changes normalized bytes requires an explicit preset version decision; do not silently change the v1 reader.

All finite float32 camera/settings values must recover the same bits except normalized zero; all string/identity/debug fields must recover their values. Test representative cross-platform byte/digest fixtures, key-order/whitespace variants, Unicode escaping, numeric boundary round trips and malformed records. These are tests of this small codec, not a new independent serialization conformance project. Digest verification, bounded parsing and atomic import/export remain required.

## Import transaction

1. Read a regular file through Core with the byte limit before allocating parser structures; enforce bounded parser depth/value count and strict UTF-8.
2. Validate schema, identities, bounds and normalized-byte digest. Build a complete candidate camera/settings value; do not mutate the current session.
3. Resolve requested output against the current display generation. An unsupported request rejects the entire import; retain the session's previous valid camera, requested/effective settings and pending ordinary request, and report the rejected intent only in diagnostics. No preset fields are applied by substituting SDR. A display-generation change invalidates preparation and requires resolution again before commit. If the session's existing effective output independently loses capability, perform the normal explicit SDR fallback or pause without importing any rejected preset fields.
4. If drawable extent is zero, retain one validated pending record, visibly marked pending. A newer explicit import supersedes it; cancel or close discards it. No preset field applies until a non-zero extent and coherent capability generation exist. While that transaction is pending, ordinary camera/output edits are visibly disabled; Cancel restores normal editing, avoiding partial combination of a preset and unrelated later edits.
5. Restore position, yaw/pitch, vertical FOV, near/far, speed and valid requested settings. Rebuild projection from the latest drawable aspect without resizing the window. Publish one settings revision and a PresetRestore discontinuity. Horizontal framing may differ from export.

Rejected import intent is diagnostic context, not the session's new requested intent. An explicit later export represents the unchanged session settings (or independently recovered output) and current source context; it does not serialize the rejected request as if it were imported. Serialization round-trip is distinguished from restoring at a different extent/display.

## Export transaction and protected paths

Export is explicit and writes only under the configured lab export root. Validate the canonical root/target and reject destinations within Accepted/baseline registries, source/cooked-content roots or formal evidence directories, including symlink escapes. Input presets are read-only. File names are generated or sanitized as a single bounded component; preset data cannot choose a destination path.

Write a sibling temporary file durably, then atomically publish with no replacement through a new Core `PublishFileNoReplace` operation. Default existing-target outcome is AlreadyExists. A user-triggered explicit overwrite may use existing `ReplaceFileAtomic`; it is never inferred from repeated export. No check-then-write race or partial final file is allowed. Implement native no-replace behind Core on Windows/macOS/Linux, with Unsupported on unavailable semantics rather than silent overwrite. Temporary cleanup is contained to the export root.

Reuse yyjson 0.12.0 as one build-only library shared by Asset/Application, with private include access and no public JSON types. Add parser allocator budgets and preserve all existing Asset JSON tests after removing its duplicate private C object. Preset hashing uses the public Asset digest value contract; the Asset module acquires no Application/UI dependency.

## Acceptance

FR-019/FR-020, US5 and SC-006. Test same/different extent and aspect, minimized pending import, display change before commit, wrong revision/source root, cross-target equivalent content, duplicate fields, deep JSON, oversized bytes, missing/unknown fields, malformed Unicode, out-of-range numeric values, digest mutation, every float field round-trip, protected paths and concurrent export-to-existing races. A preset test never updates an Accepted registry.
