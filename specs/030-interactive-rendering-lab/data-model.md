# Data Model: Interactive Rendering Lab

**Status**: Proposed design; value names are implementation targets. Numeric/byte limits are authoritative in [lab-runtime.md](contracts/lab-runtime.md).

## 1. FInteractiveLabSession

**Owner**: Application. **Identity**: monotonic SessionId, one native window, one loaded workload.

Fields: State, WorkloadIdentity, CurrentCamera, EffectiveSettings, RequestedSettings, PendingOutputRequest, PendingPreset, InputOwnership, UIAvailability, DisplayState, Diagnostics. Holds services through separate ownership boundaries; it does not parse JSON, implement widgets or own native graphics objects.

Transitions: Starting -> Ready -> Running; Running <-> PausedZeroExtent; Running -> TransitionPending -> Draining -> Running; any live state -> Failed/Closed. A UI failure changes UIAvailability without declaring scene failure when scene-only preparation is still valid. Closed releases all process-local state. No automatic persistence.

Validation: one workload/window, source identity validated through strict-cooked runtime; session state cannot publish a frame using a stale display/settings generation.

## 2. FFreeCameraState / FCameraChangeSet

**Owner**: Application. **Identity**: CameraRevision within session.

Fields: Position, YawRadians, PitchRadians, VerticalFovRadians, NearPlane, FarPlane, MovementSpeed, DrawableExtent, View, Projection, ViewProjection. Pose/projection parameters are authoritative; matrices are derived and validated. Reset data is workload-owned, but interactive aspect is current drawable aspect.

Change flags: ContinuousMotion, Cut, Reset, PresetRestore, ProjectionChanged, ExtentChanged; include previous/new extent and revision. FOV/near/far changes invalidate future projection histories. No temporal history, motion-vector buffer or jitter exists in this phase.

Validation: finite values, positive-X StandardZ and bounds in camera-input contract. At zero drawable extent, no projection division or partially applied preset. Same-workload import restores position/orientation and vertical FOV without resizing the window.

## 3. FInputOwnershipSnapshot / FOrderedLabEvent

**Owner**: Application input router. **Identity**: EventSequence and FocusGeneration.

Event fields: type, timestamp, sequence, focus generation, physical key/button or Unicode scalar, logical pointer/scroll data, extent/scale or focus payload. Cross-kind order is preserved; close/focus-loss latches survive queue overflow.

Ownership fields: KeyboardOwner, PointerGestureOwner, ScrollOwner, PressOwnerByKey/Button, QuarantinedHeldKeys, CursorMode, PointerBaselineValid, Focused. Owner values: None, UI, Viewport. UI receives raw events independently of camera gating. A drag remains owned through release/cancel; transfer-to-UI quarantines held navigation keys.

Lifecycle: native poll -> raw state -> private UI frame/activation -> ownership resolution -> candidate camera update -> snapshot commit. Focus/capture loss clears baselines and re-arms only on new presses. Events do not persist across sessions.

## 4. FWindowDisplayState

**Owner**: Application window service; Renderer receives a copied value.

Fields: LogicalExtent, DrawableExtent, ContentScale, FramebufferScale, DisplayGeneration, Focused, Minimized. Native output capabilities remain behind Renderer/RHI; Application sees valid/unsupported output choices through an engine value snapshot.

Invariant: drawable/client ratio drives scissor/hit mapping; scale generation changes invalidate stale packet/preset/transition preparation. Zero drawable pauses drawing and uploads but leaves event service live.

## 5. FLabSettingsSnapshot / FLabOutputRequest

**Owner**: Application selects intent; Renderer resolves output policy. **Identity**: SettingsRevision, RequestId, DisplayGeneration.

Fields: CameraRevision, RequestedProfileId, EffectiveProfileId, SdrToneMapVersion, HdrViewingVersion, ExposureStops, DebugBypass (Mode, StageName, SourceDomain, VisualizationMinimum, VisualizationMaximum), UIVisible, UIWhiteMultiplier, UIReferenceWhiteNits, NativePackingWhiteNits, OutputModeGeneration. UIReferenceWhiteNits and packing white derive from one generation but carry distinct semantic names.

Request states: Requested -> Pending -> Preparing -> Effective or Rejected/TimedOut. Latest valid pending request supersedes older pending intent; it does not mutate in-flight snapshots. Unknown versions/out-of-range settings reject. Capability loss explicitly chooses supported SDR or pauses. Retain requested intent and failure reason; never report an unsupported profile as effective.

## 6. FUIDrawSnapshot / FUIVertex / FUIDrawCommand

**Owner**: Application constructs copied values; Renderer validates/prepares GPU resources. **Identity**: SessionId, FrameId, SettingsRevision and DisplayGeneration.

Snapshot fields: logical DisplayPos/Size, FramebufferScale, vertex array, uint32 index array, command array, retained texture-generation identities. Vertex carries Position, UV, sRGB RGBA8. Command carries FirstIndex, IndexCount, BaseVertex, ClipRect, TextureId, Draw/ResetState.

Invariant: immutable after publication; every referenced texture generation is live from snapshot preparation until all upload/render consumers complete and all prepared/queued snapshot leases release; presentation release does not extend these sampling leases; all byte arithmetic and ranges validate before native recording. Empty or fully clipped draws are legal no-ops. Arbitrary callback pointers are prohibited.

## 7. FUITextureId / FUITextureRequest / FUITextureRecord

**Owner**: Renderer GPU record; Application-private adapter owns the associated ImGui request state. **Identity**: Slot + Generation; request correlation has monotonic RequestId.

Fields: source kind (CPUUpload or RendererProduced), dimensions, RGBA8 pixels/CPU shadow for CPUUpload or a Renderer-private RHI generation/producer-pass lease for RendererProduced, source color domain, readiness dependency, reference count/leases, last submission token, state and byte counts. Font source bytes remain owned for dynamic baking lifetime. Logical IDs never contain native handles.

State: Requested -> Prepared -> UploadQueued -> Ready -> Retiring -> Destroyed. Updates create a new generation, preserving old leased generations. Failed requests do not change the current identity or acknowledge upstream completion. Retirement follows actual completion and obeys total byte/record budgets. RendererProduced registrations skip CPU upload/shadow states, become sampleable only with a valid producer-before-UI graph dependency and use the same slot/generation/render-lease retirement rules without ImGui request acknowledgements.

## 8. FLabFrameSlot / FDeferredSubmissionRecord / FPresentationBinding

**Owner**: Demo coordinates slot records through FLabProductionFrameContext; Renderer owns/realizes slot resources; Backend owns native submission and acquired drawable lifetime. **Identity**: slot index 0/1, FrameToken, SubmissionToken, OutputModeGeneration.

Slot fields: command buffer/fence, camera/output uniforms, graph attachments, immutable UI snapshot leases, texture readiness dependencies, settings/display revisions, optional capture request. PresentationBinding wraps a borrowed RHI target plus acquire/present synchronization and generation; no raw platform handle appears in public Application contracts.

Frame states: Free -> Recording -> Submitted -> PresentationQueued -> RenderCompleted -> Retired. Separate presentation lease states are Acquired -> PresentQueued -> Released; the acquired image index identifies its semaphore/lease independently of the two slot indices. SubmitDeferred acceptance does not imply completion. Native render fence results drive frame-resource retirement; backend presentation fences or proven acquired-image dependencies drive separate image/semaphore lease reuse. Resize/close follows the selected backend-neutral retirement mode: Vulkan queries optional maintenance1 and chooses PresentationFence or AcquireHistory. Fallback retains at most one active and one retiring generation, with <=16 total image/lease records and <=512 MiB estimated swapchain color storage. Logical cancellation may keep an acquired unpresented image owned until teardown. Record advertised/enabled support, selection reason and shutdown assurance (Proven, IdleAssumed, Forced or DeviceLost); ordinary render completion never proves present release. Terminal fallback cleanup may use the explicitly qualified idle assumption; forced termination/device loss remains failure. See contracts/lab-runtime.md for the complete state/budget rules. Presentation-queued is not physical scanout. Vulkan persistent records retain native resources beyond SubmitDeferred return; Metal uses completion-handler ownership. Existing formal synchronous facade remains a separate caller.

## 9. FLabPreset

**Owner**: Application-private codec and session. **Identity**: schema/version, workload tuple and normalized yyjson-writer SHA-256.

Wire shape is specified in [lab-preset.md](contracts/lab-preset.md). Camera position, yaw/pitch, vertical FOV, near/far and movement speed are restorable values. The full debug mode/stage/domain/range object is restored and validated without creating a readback request. Exported drawable extent, backend, cooked generation and software revision are source context. Identity match uses workload revision, production root and target-independent source closure digest; changing aspect/backend alone is not a mismatch.

Lifecycle: read bounded bytes -> validate entire record/digest -> resolve capabilities (reject unsupported intent without changing current valid state) -> pending if zero extent -> revalidate generation -> coherent commit or reject. Export uses durable sibling temporary bytes and atomic no-replace; overwrite is an explicit action. No automatic Accepted/baseline update or formal-preset authority exists.

## 10. FLabCaptureRequest / FLabValidationRecord

**Owner**: session requests capture; Renderer/backend owns bounded readback; validation tooling owns evidence serialization.

Capture fields: RequestId, target settings/display generation, IncludeUI, purpose, output context, frame/readback token, deadline and result. Queue includes pending and active requests, maximum two. Without a request, readback resources and operations are absent from native execution.

Validation fields: schema, evidence kind and coverage case ID (smoke/endurance/lifecycle), software/device/workload identity, exact extents/profile/white, coverage results, submitted/queued/completed counts, readback/resource counters, limits version, artifact digests and first failure. A separate human record contains reviewer observations tied to a request; automation cannot fill positive observations.

## Relationships and invariants

One session owns one current camera and one effective settings snapshot. Each of two frame slots retains exactly the revisions it recorded. Multiple slots may lease one immutable scene/texture generation. One pending output request and one pending preset are independent candidates, but only the session publishes a coherent combined revision; preset commit invalidates conflicting pending ordinary settings intent through an explicit superseded result. A later user edit may supersede an uncommitted preset only through cancel/new import, not partially edit its contents. Formal validation bypasses all live session state.

All transient identities are process-local monotonically increasing values with invalid zero; stale or wraparound identities reject/restart the session rather than alias a live record. Cross-process authority uses full software/content/artifact digests, not transient IDs. See [validation-evidence.md](contracts/validation-evidence.md) for closeout semantics.
