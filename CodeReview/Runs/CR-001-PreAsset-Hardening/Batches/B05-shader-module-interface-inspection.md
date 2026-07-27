# B05-S07: Shader Modules And Interfaces Inspection

## Scope

This packet inspected one Feature 012 responsibility domain across eight
primary production files:

- `FRHIShaderModuleDesc.h`;
- `FRHIPipelineLayoutDesc.h`;
- `FVulkanShaderModule.h/.cpp`;
- `FVulkanPipelineLayout.h/.cpp`;
- bounded shader/pipeline factory paths in `FVulkanDevice.cpp`;
- the runtime-mode boundary in `FVulkanInstance.cpp`.

The inherited 22-line `IRHIShaderModule` declaration, bounded device ownership
members, Feature 008/012 authority documents, maintained RHI/Vulkan tests, and
later native shader call sites were read as supporting contract context. No
production source or maintained test changed.

## Authority And Invariants

Feature 008 FR-017 and Feature 012 FR-001 through FR-003a, FR-018, SC-001,
SC-002, T034-T035, and the Shader Module Contract require:

- an active device to remain the authoritative creator and owner of shader
  modules and pipeline layouts;
- structurally malformed, wrong-stage, missing-entry-point, and
  metadata-incompatible shader payloads to fail explicitly without a usable
  partial module;
- deterministic fallback to perform meaningful lightweight structural checks;
- a backend with an available real runtime to rely on real runtime shader
  creation validation;
- shader/layout dependencies to remain queryable, compatible, and invalidated
  through their owning device.

The verified B04 runtime correction is also authoritative: deterministic
fallback must never be reported as native Vulkan availability.

## Findings

### CR001-B05-F007 - S2 Accepted

`IsValidRHIShaderBytecode` accepts any four-word array whose first word is the
SPIR-V magic number and whose format string is `SPIR-V`. It does not validate a
complete binary header, instruction word counts, an `OpEntryPoint`, entry-point
name, or the execution model corresponding to the declared RHI stage.

The maintained shader helpers use exactly four synthetic words for their
positive vertex, fragment, and compute fixtures. Those fixtures have no
complete SPIR-V header or entry-point instruction, yet both mock and Vulkan
factories publish them as valid shader modules and use them for successful
pipeline creation. The malformed and wrong-stage requirements are therefore
not represented by the current green tests.

### CR001-B05-F008 - S2 Accepted

`FVulkanShaderModule` and `FVulkanPipelineLayout` expose public constructors and
carry no creating-device identity. Graphics and compute pipeline validation
checks dynamic backend type, lifecycle, stage, and metadata compatibility, but
not provenance. A wrapper constructed directly or returned by another active
`FVulkanDevice` can therefore be composed into the receiving device's
pipeline.

`FVulkanDevice::CreateShaderModule` also performs `MakeShared` and ownership
vector insertion without mapping allocation or tracking failure to an
explicit RHI result. This is one ownership/factory finding: closing
construction, attaching provenance, validating dependencies, and making
publication failure-atomic are one bounded API migration.

### CR001-B05-F009 - S2 Accepted

B04-F001 correctly made `FVulkanDevice` reject its former false native mode.
That correction exposes a remaining Feature 012 gap: every
`FVulkanInstance::RealRuntime` request now returns `Unsupported`, so the only
active `FVulkanDevice` path is deterministic fallback.

`FVulkanDevice::CreateShaderModule` contains no native shader creation and its
`Runtime` validation branch is unreachable. `FVulkanNativeContext` and
`FVulkanNativeOffscreenSession` do call `vkCreateShaderModule`, but those
objects do not implement `IRHIDevice` or return `IRHIShaderModule`. Real
rendering and the backend-neutral shader factory therefore remain separate
authorities.

## Interface Metadata Review

The metadata and pipeline-layout helpers correctly reject:

- undefined descriptor types and stage-flag bits;
- zero descriptor array counts;
- duplicate set/binding pairs;
- overflowed or same-stage overlapping constant ranges;
- missing, wrong-type, undersized, or insufficiently visible layout bindings;
- constant ranges not contained by a compatible layout range.

No separate metadata finding was opened. The material defect is that metadata
can currently be attached to bytecode that was never shown to contain the
declared stage or entry point.

## Coverage Gap And Impact

Maintained tests cover empty/short bad magic, unsupported RHI stage, missing
entry-point text, invalid metadata domains, overlap, layout mismatch,
deterministic diagnostics, and shutdown invalidation.

They do not cover a complete malformed SPIR-V instruction stream, execution
model or entry-point mismatch, cross-device shader/layout composition, direct
construction closure, shader factory allocation rollback, or an RHI
real-runtime shader object. Future Feature 024 shader assets would otherwise
cache and distribute payloads behind a validation result that does not prove
the payload or its owning backend.

## Validation

- exact static control-flow review within the bounded shader/interface domain;
- requirement comparison against Features 008 and 012;
- comparison with maintained synthetic fixtures and later real native shader
  call sites;
- the exact production source state already passed fresh strict Debug,
  fallback strict full tests, and strict Release in B05-S06;
- no custom executable, fault trigger, debugger, sanitizer, remote CI, or
  network activity was used;
- no production or maintained-test source changed.

## Handoff To B05-S08

B05-S08 should repair the three accepted findings as one shader ownership and
runtime integration migration:

1. replace magic-only validation with bounded SPIR-V structure plus declared
   entry-point/execution-model validation, and replace synthetic positive
   fixtures with valid repository-owned or minimal generated payloads;
2. make the device authoritative for shader/layout construction, attach shared
   owner provenance, reject cross-device pipeline dependencies, and map
   factory allocation/tracking failures to explicit results;
3. connect the RHI shader path to one owner-safe native Vulkan authority so
   `RealRuntime` creates and destroys real shader modules, while explicit
   fallback remains structurally validated and observably non-native;
4. add maintained regressions for malformed instruction streams, wrong stage
   and entry point, provenance, construction closure, rollback, native success,
   native rejection, and shutdown destruction.

No push or GitHub Actions run occurred in this packet.
