# B03-S05: Commands, Queues, Synchronization, And Swapchain Fix

## Scope

This packet fixes the three Accepted S2 findings from B03-S04:

- `CR001-B03-F003`: surface-aware swapchain creation silently falls back to a
  headless frame-count factory.
- `CR001-B03-F004`: failed queue and swapchain synchronization operations can
  leave partial state transitions.
- `CR001-B03-F005`: the clear-value render-pass overload can report success
  while discarding clear semantics.

The production change is limited to three RHI public headers. Maintained
acceptance behavior and regressions remain in `Tests/RHICoreTests.cpp`. No
Vulkan backend implementation, Renderer path, feature specification, or
Feature 020 asset code changed.

## Implementation

### Fail-Closed Compatibility Defaults

The following default overloads now return `Unsupported` without invoking a
legacy operation:

1. `IRHIDevice::CreateSwapchain(surface, desc)`
2. `IRHICommandBuffer::BeginRenderPass(..., clearValues)`
3. `IRHISwapchain::AcquireNextFrame(..., signalSemaphore)`
4. `IRHISwapchain::Present(..., waitSemaphore)`

Legacy implementations therefore cannot claim semantics they do not
explicitly implement. Existing mock and Vulkan clear-value implementations
continue to override the explicit-clear overload.

### Deterministic Swapchain Synchronization

The mock swapchain explicitly implements synchronized acquire and present:

- unknown synchronization implementations return `Unsupported`;
- an already-signaled acquire semaphore is rejected before image acquisition;
- acquire writes the caller's image index only after image and semaphore state
  can commit together;
- present validates swapchain state and image identity before consuming its
  wait semaphore;
- resize, unavailable, invalid-image, and not-ready outcomes leave unrelated
  synchronization state unchanged.

### Queue Submission Preflight

The mock queue now validates the complete submission before consuming or
signaling anything:

- command buffer ownership, state, and queue compatibility;
- every wait semaphore, including readiness and duplicates;
- every signal semaphore, including already-signaled, duplicate, and
  wait/signal overlap cases;
- completion fence ownership and unsignaled state.

Only a fully valid submission consumes waits, marks the command submitted,
records the submission, signals outputs, and signals the fence.

## Regression Coverage

Maintained tests now prove:

- surface and clear compatibility defaults fail closed through base
  references without calling legacy methods;
- synchronized swapchain compatibility defaults do not mutate legacy objects;
- failed synchronized acquire preserves output index, swapchain state, and
  semaphore state;
- failed synchronized present preserves the acquired image and wait signal;
- successful acquire/present commits both sides of each state transition;
- a later unready wait does not consume an earlier ready wait;
- an invalid output signal does not submit the command, increment queue count,
  or signal the fence.

## Local Verification

- Strict standalone current-contract probe: passed.
- Focused maintained assertions: 10 passed.
- Full graphics-disabled deterministic suite: passed, 770 result lines and no
  failure record.
- Strict Debug build: passed.
- Strict Release build: passed.
- Strict ASan/UBSan build and test suite: passed.
- `git diff --check`: passed.

Authoritative output and gate references are in
`Evidence/b03-command-sync-swapchain-fix-probes.md`.

## API Migration

This is an intentional fail-closed correction. A derived class that previously
relied on a semantic-loss default now receives `Unsupported` and must override
the richer overload before advertising support. Repository call-site search
found no active production caller of the affected surface/synchronized
swapchain defaults, and all production explicit-clear calls target an
overriding implementation.

## Finding State

- `CR001-B03-F003`: Fixed at `b29f466`.
- `CR001-B03-F004`: Fixed at `b29f466`.
- `CR001-B03-F005`: Fixed at `b29f466`.

Independent parent reproduction and current-code verification remain
B03-S06's responsibility. No push or GitHub Actions run was requested in this
packet.

