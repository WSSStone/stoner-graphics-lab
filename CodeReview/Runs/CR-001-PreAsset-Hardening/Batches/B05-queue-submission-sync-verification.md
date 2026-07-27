# B05-S06: Queue, Submission, And Synchronization Verification

## Verification Target

This packet independently verifies the repairs committed at `3dbfed0`:

- `CR001-B05-F004`: atomic queue-submission validation and commit;
- `CR001-B05-F005`: recoverable, idempotent completion transitions;
- `CR001-B05-F006`: device provenance, construction authority, and shutdown
  invalidation for synchronization objects.

No production source or maintained test implementation changed during this
verification packet.

## Authority

Feature 011 requires deterministic command submission, explicit wait/signal
semantics, recoverable timeout behavior, bounded object ownership, lifecycle
invalidation, and positive and negative synchronization coverage.

The accepted findings further require invalid submissions to preserve every
input object and the successful-submission count, and require objects from
different devices to reject composition.

## Parent And Current Comparison

The exact implementation parent `2f30cfd` was reviewed from Git without
executing historical code. The current implementation was then traced from
the device factories through queue submission and completion.

Current-source review confirms:

- the complete command, wait, signal, and fence set is preflighted before
  tracking allocation or mutation;
- duplicate waits, duplicate signals, wait/signal overlap, stale objects,
  foreign-device objects, unready waits, and unsignalable outputs are rejected;
- accepted submission publishes its tracking record before the no-fail commit
  transitions and increments the successful count last;
- completion first makes the command buffer resettable and only then publishes
  `Completed`;
- `NotReady` and `Timeout` observations remain recoverable through
  `WaitIdle`, and observing an already completed submission is idempotent;
- device factories are the construction authority for queues, command pools,
  fences, and semaphores, while pools own command-buffer construction;
- all of those wrappers share one active owner identity, and device shutdown
  invalidates that owner before traversing retained objects;
- wrapper or tracking allocation failures return `Unavailable` without
  publishing a partial object.

## Finding Verification

### CR001-B05-F004

Maintained tests exercise a later unready wait, an already-signaled output,
duplicate waits, wait/signal overlap, and foreign-device inputs. Every
rejection preserves semaphore, fence, command-buffer, and submission-count
state. The accepted path commits only after all checks and tracking allocation
succeed.

### CR001-B05-F005

Maintained tests exercise nonzero-timeout success, `NotReady` followed by
`WaitIdle`, and `Timeout` followed by `WaitIdle`. Each successful completion
leaves the command buffer resettable, and subsequent observation returns
success without repeating transitions.

### CR001-B05-F006

Maintained tests reject foreign command buffers and semaphores, and verify
shutdown invalidation. Header and factory review confirms external callers
cannot directly construct the device-owned synchronization wrappers or invoke
their submission-only mutation helpers.

## Local Gate Evidence

Fresh predefined ordinary gates passed at the verification source state:

- strict Debug build;
- fallback strict Debug build plus the full maintained deterministic suite;
- strict Release build.

Detailed outputs are stored in the generated gate JSON files and summarized in
`Evidence/b05-queue-submission-sync-verification.md`.

The separately tracked intermittent native deferred-rendering assertions
remain owned by accepted finding `CR001-B08-F001`; they do not affect the
deterministic queue/synchronization verification in this packet.
