# Contract: Request, Operation, And Retention Lifecycle

## Admission

1. Validate manager state, handle output, AssetId/type, limits, and mode evidence.
2. For a callback-bearing request, reserve one completion slot; reject without
   mutation if capacity is exhausted.
3. Allocate a generation-safe request slot.
4. Build the complete load key.
5. Attach to an existing operation/cache entry or create one operation.
6. Queue work and return the caller's handle.

Admission rollback removes every partial slot/interest/count.

## State Transitions

| Current | Event | Next |
|---|---|---|
| Accepted | closure work admitted | WaitingForDependencies |
| Accepted/Waiting | no dependencies and physical load starts | Loading |
| Waiting | all required nodes Ready | Loading or Ready |
| Loading | payload and closure commit atomically | Ready |
| Any non-terminal | stable error/required dependency failure | Failed |
| Any non-terminal | caller cancel with no published success | Cancelled |
| Ready/Failed/Cancelled | any later event | unchanged |

One caller's terminal state does not force another caller sharing the operation
to the same cancellation state. Physical success publishes only to active
interests.

## Coalescing

- At most one active physical operation exists per complete load key.
- A cache hit and an in-flight coalescing hit are distinct inspection decisions.
- Non-equivalent expected type, target digest, mode, or generation cannot share.
- Operation completion is committed once; each caller receives an independent
  result and completion sequence.

## Dependency Failure

Required failure produces a bounded root-to-failure path. Cycle paths begin and
end with the repeated node. Shared operations may supply different root paths.
Optional failure is tolerated only under an explicit owning payload fallback;
the selected fallback appears in inspection.

## Retention Accounting

| Class | Increment | Decrement |
|---|---|---|
| External handle | successful handle creation/copy control | final shared handle control release |
| Request interest | active request or queued undelivered result | request release/cancel delivery |
| Required dependency | Ready root retains closure | root retention ends |

All arithmetic is checked. An entry is removed immediately at `0/0/0`. No
timer, trim, LRU, capacity-pressure eviction, or shutdown grace period exists.
Completion reservations are independent bounded admission resources and are
released exactly once by dispatch or request-interest release.

## Cancellation

Cancellation marks only that request interest. Work receives a cooperative stop
signal when no interest class needs it. A non-interruptible extension call may
finish, but commit discards its result if no interest remains. Cancellation is
idempotent and never turns a published typed handle into invalid memory.

## Shutdown

Shutdown order is admission close -> queued cancellation -> running-work join ->
terminal-state audit -> manager retention release -> generation lease release.
No worker may reference manager state after Shutdown returns.
