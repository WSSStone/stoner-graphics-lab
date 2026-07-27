# B05-S04: Queue Submission And Synchronization Inspection

## Scope

This packet inspected one Feature 011 responsibility domain across exactly
eight production files, 542 lines total:

- `FVulkanQueue.h/.cpp`;
- `FVulkanCommandSubmission.h/.cpp`;
- `FVulkanFence.h/.cpp`;
- `FVulkanSemaphore.h/.cpp`.

The review also read bounded device factory call sites, Feature 009 and 011
authority documents, the maintained RHI mock queue, and maintained Vulkan
tests. No production source or maintained test changed.

## Authority And Invariants

Feature 009 FR-009 and FR-017 and Feature 011 FR-013 through FR-017 require:

- queues and synchronization objects to be created and owned by an active
  backend device and invalidated by its shutdown;
- a queue to accept only an ended, executable command buffer compatible with
  the queue and synchronization set;
- wait semaphores to be consumable and signal semaphores and fences to be
  signalable before a submission is accepted;
- rejected submission validation to preserve successful-submission
  observability and unrelated object state;
- successful fallback completion and wait-idle to make submitted command
  buffers resettable;
- not-ready, timeout, invalid, and failed completion outcomes to remain
  explicit and consistent with the command lifecycle.

The maintained RHI queue preflights the complete wait, signal, and fence set,
including duplicate and overlap checks, before mutating any object.

## Findings

### CR001-B05-F004 - S2 Accepted

`FVulkanQueue::Submit` consumes wait semaphores sequentially before validating
the complete wait set. It then marks the command submitted and increments the
successful count before validating all signal semaphores and the fence. A
later invalid or unready object therefore returns failure after earlier
objects, command state, or observability have already changed.

The maintained RHI regressions explicitly require a later unready wait and an
already-signaled output to leave every input, command, count, and fence
unchanged. The Vulkan tests cover only the successful synchronization path.

### CR001-B05-F005 - S2 Accepted

`FVulkanCommandSubmission::ObserveCompletion` reports `Success` and stores
`Completed` for any nonzero timeout without making the command buffer
resettable. Forced `NotReady` and `Timeout` observations store terminal-looking
states, while `FVulkanQueue::WaitIdle` only visits `Pending` submissions.

Consequently a caller can observe completion while reset still fails, and a
single retryable observation can strand the command in `Submitted` even after
queue wait-idle reports success.

### CR001-B05-F006 - S2 Accepted

Queue and fence constructors are public, semaphore has a public implicit
default constructor, and none of these objects carries a creating-device
identity. Submission checks concrete backend type but not device provenance.

Callers can therefore bypass device capability and tracking factories, keep
directly constructed objects active across device shutdown, or submit concrete
commands and synchronization objects created by different Vulkan devices.
This contradicts the documented device-created/device-owned boundary.

## Factory And Transition Review

The ordinary device shutdown loop invalidates queues, fences, and semaphores
that were registered by the device factory. Fence and semaphore methods reject
operations after that invalidation, and semaphore transition checks reject
double signal or consume-before-signal. Those local transitions are coherent
for factory-created objects used by one device.

Queue, fence, and semaphore factories do not currently map wrapper or tracking
allocation failures to explicit `Unavailable` results. This is part of
CR001-B05-F006 rather than a fourth finding because closing construction,
adding owner identity, and making the same factories failure-atomic form one
ownership migration.

## Coverage Gap And Impact

Maintained Vulkan tests cover:

- ordinary fence signal/wait/reset and semaphore signal/consume/reset;
- successful fallback submission with one wait, one signal, and one fence;
- incompatible queue rejection;
- one not-ready observation, one timeout observation, and ordinary wait-idle;
- device-shutdown invalidation for factory-created objects.

They do not cover full-set preflight atomicity, duplicate or overlapping
semaphores, successful nonzero-timeout completion, recovery after not-ready or
timeout, cross-device provenance, direct-construction closure, or factory
allocation rollback. These gaps can strand reusable command buffers and make
frame synchronization state depend on where validation fails.

## Validation

- exact static control-flow review for all eight scoped production files;
- direct comparison with maintained RHI preflight and atomicity regressions;
- authority comparison against Feature 009 and 011 specs, data models, and
  submission contract;
- no custom executable, fault trigger, debugger, sanitizer, remote CI, or
  network activity was used;
- no production or maintained-test source changed.

## Handoff To B05-S05

B05-S05 should repair the three accepted findings as one bounded queue and
ownership migration:

1. preflight the complete command, wait, signal, and fence set before mutation,
   including concrete type, validity, duplicates, overlap, signalability, and
   creating-device provenance;
2. publish submission tracking, command state, count, outputs, and fence only
   after all validation and required allocations succeed;
3. ensure every successful completion makes the command resettable and
   retryable outcomes remain recoverable by observation or wait-idle;
4. close queue/fence/semaphore construction to the device, attach shared owner
   identity, and map factory allocation/tracking failures to explicit results;
5. add maintained regressions for atomic failure, completion recovery,
   provenance, construction closure, and factory rollback.

No push or GitHub Actions run occurred in this packet.
