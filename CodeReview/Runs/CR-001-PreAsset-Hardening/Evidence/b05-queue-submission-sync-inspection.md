# B05-S04 Queue Submission And Synchronization Evidence

## Packet

- Step: `B05-S04`
- Review mode: static source, authority, and maintained-test comparison
- Production scope: 8 files, 542 lines
- Production changes: none
- Maintained-test changes: none

## Requirement Evidence

| Authority | Required invariant |
| --- | --- |
| `specs/009-vulkan-device-swapchain/spec.md` FR-009, FR-017 | Synchronization objects satisfy RHI lifecycle contracts; device shutdown invalidates owned queues and synchronization objects. |
| `specs/009-vulkan-device-swapchain/data-model.md` | Queue is owned by backend device; fences and semaphores are created by backend device. |
| `specs/011-vulkan-commands-submission/spec.md` FR-013 through FR-017 | Submission validates executable compatibility, reports consistent observability/completion, and rejects work after invalidation. |
| `specs/011-vulkan-commands-submission/data-model.md:203` | Waits must be consumable, outputs signalable, and failed validation must not increment successful count. |
| `specs/011-vulkan-commands-submission/data-model.md:228` | Successful completion makes submitted command buffers resettable. |
| `specs/011-vulkan-commands-submission/contracts/vulkan-command-submission-contract.md` | Failed submissions preserve count; fallback completion and wait-idle expose consistent command state. |

## Control-Flow Matrix

| Case | Current Vulkan path | Required/maintained behavior | Finding |
| --- | --- | --- | --- |
| Second wait is unready | First wait is consumed before second wait returns `NotReady`. | Preflight all waits; first wait remains signaled and command/count remain unchanged. | CR001-B05-F004 |
| Output is already signaled | Command becomes `Submitted` and count increments before output rejects. | Reject before mutation; command remains completed, count stays zero, fence remains unsignaled. | CR001-B05-F004 |
| Duplicate or wait/signal overlap | No full-set identity check; sequential mutation decides the result. | Reject duplicate waits, duplicate outputs, and wait/output overlap during preflight. | CR001-B05-F004 |
| Observe with nonzero timeout | Stores `Completed`, returns `Success`, leaves command `Submitted`. | Successful completion makes command resettable. | CR001-B05-F005 |
| Observe forced not-ready/timeout, then wait-idle | State becomes `NotReady`/`Timeout`; wait-idle skips every non-`Pending` entry. | Retryable completion remains recoverable and wait-idle completes valid fallback work. | CR001-B05-F005 |
| Direct wrapper construction | Queue/fence/semaphore can exist outside device tracking. | Objects are created and owned by an active device and invalidated at shutdown. | CR001-B05-F006 |
| Cross-device concrete objects | Backend casts succeed; no device provenance is compared. | Queue accepts only objects belonging to its creating device. | CR001-B05-F006 |
| Factory allocation/tracking failure | Queue/fence/semaphore factories do not map allocation exceptions or roll back tracking publication. | Factory returns explicit failure without a usable partial object. | CR001-B05-F006 |

## Precise Source Locations

### CR001-B05-F004

- `Source/Backend/Vulkan/Private/FVulkanQueue.cpp:67-91`: submission record
  allocation and publication precede synchronization validation.
- `Source/Backend/Vulkan/Private/FVulkanQueue.cpp:93-106`: waits mutate one by
  one.
- `Source/Backend/Vulkan/Private/FVulkanQueue.cpp:108-134`: command and count
  mutate before all outputs and fence are accepted.
- `Tests/RHICoreTests.cpp:1769-1809`: maintained atomic failure expectations.

### CR001-B05-F005

- `Source/Backend/Vulkan/Private/FVulkanCommandSubmission.cpp:20-46`:
  nonzero-timeout success omits command completion; injected retryable outcomes
  replace `Pending`.
- `Source/Backend/Vulkan/Private/FVulkanQueue.cpp:143-159`: wait-idle completes
  only `Pending` submissions.
- `Tests/VulkanBackendTests.cpp:1089-1127`: maintained Vulkan coverage stops
  after observing not-ready/timeout and does not verify recovery.

### CR001-B05-F006

- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanQueue.h:13-15`: public queue
  constructor.
- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanFence.h:10-12`: public fence
  constructor.
- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSemaphore.h:8-17`: implicit
  public default constructor.
- `Source/Backend/Vulkan/Private/FVulkanDevice.cpp:301-315,363-385`: device
  factories track objects but do not provide an owner identity or explicit
  allocation-failure mapping.

## Maintained Coverage Assessment

The RHI mock queue at `Tests/RHICoreTests.cpp:1026-1180` supplies the existing
full-set preflight model. Vulkan tests validate ordinary local transitions but
omit the matrix rows above. B05-S05 should add the missing cases to maintained
tests; this packet intentionally did not create a standalone probe.

## Safety And Execution Record

This evidence was produced by reading project source, specs, and maintained
tests. No custom executable was built or run, no fault was deliberately
triggered, and no debugger, sanitizer, remote CI, network access, or external
target was used.
