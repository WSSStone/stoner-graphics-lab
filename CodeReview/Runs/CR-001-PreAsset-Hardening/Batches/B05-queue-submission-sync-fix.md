# B05-S05: Queue Submission And Synchronization Fix

## Repair Target

Implementation commit `3dbfed0` repairs:

- `CR001-B05-F004`: Vulkan queue submission mutated synchronization and
  command state before full validation;
- `CR001-B05-F005`: submission completion could diverge from the command
  lifecycle or become unrecoverable after not-ready/timeout;
- `CR001-B05-F006`: queue and synchronization wrappers bypassed device
  construction, provenance, and shutdown ownership.

The change remains inside the deterministic Vulkan queue, command,
synchronization, and device factory path plus maintained backend tests. It
does not alter native Vulkan submission, Renderer policy, or Feature 020
Asset code.

## Failure-Atomic Submission

`FVulkanQueue::Submit` now validates the entire request before any observable
state transition:

- command buffer concrete type, executable state, queue compatibility, and
  creating-device identity;
- each wait semaphore's concrete type, owner, validity, readiness, and
  uniqueness;
- each signal semaphore's concrete type, owner, validity, signalability,
  uniqueness, and non-overlap with the wait set;
- optional fence concrete type, owner, validity, and unsignaled state.

Submission tracking allocation and publication still occur before mutation
and map allocation/capacity failure to `Unavailable`. After preflight and
tracking succeed, only no-fail internal state commits remain: command
submission, wait consumption, output/fence signaling, and successful-count
increment. The count is incremented last.

## Completion Recovery

Completion observation and queue wait-idle now share one transition. The
submission publishes `Completed` only after the command buffer becomes
`Resettable`; an unsuccessful transition publishes `Failed`.

Injected `NotReady` and `Timeout` remain observable but recover through
wait-idle. A nonzero completion timeout succeeds immediately for deterministic
fallback and makes the command resettable. Once completed, later observation
is idempotently successful even if the submission retains an injection
configuration.

## Device Ownership

An `FVulkanDeviceOwnerState` is shared by queues, command pools/buffers,
fences, and semaphores created by one device. Constructors are private to the
device or command-pool owner, and queue submission compares owner identity
before accepting concrete backend objects.

Device shutdown deactivates the shared owner before invalidating tracked
objects. Direct construction and cross-device composition can no longer
bypass capability checks or shutdown invalidation.

Queue, fence, and semaphore factories now catch wrapper and tracking
allocation/capacity failures, invalidate any unpublished temporary object,
and return `ERHIResult::Unavailable`.

## Maintained Coverage

`Tests/VulkanBackendTests.cpp` now proves:

- queue, fence, and semaphore construction remains owner-only at compile time;
- a later unready wait preserves earlier waits, command state, and submitted
  count;
- an already-signaled output preserves the command, count, and fence;
- duplicate waits and wait/signal overlap reject without mutation;
- foreign commands and synchronization objects reject by device provenance;
- successful nonzero-timeout completion makes the command resettable;
- not-ready and timeout outcomes recover through wait-idle and later
  observation remains successful.

## Local Verification

Fresh local results are recorded in
`Evidence/b05-queue-submission-sync-fix.md`:

- strict Debug build: passed with project warnings treated as errors;
- strict deterministic fallback build and complete maintained tests: passed;
- strict Release build: passed with project warnings treated as errors;
- `git diff --check`: passed.

The graphics-enabled `tests` profile encountered the existing
`CR001-B08-F001` intermittent MoltenVK deferred-native failure. Five bounded
repetitions isolated the same three native readback assertions in one run;
all queue/synchronization regressions passed on every run. This packet does
not rewrite B08 native validation.

## Finding State

- `CR001-B05-F004`: Fixed at `3dbfed0`.
- `CR001-B05-F005`: Fixed at `3dbfed0`.
- `CR001-B05-F006`: Fixed at `3dbfed0`.

Independent verification remains assigned to B05-S06. No push or GitHub
Actions run occurred in this packet.
