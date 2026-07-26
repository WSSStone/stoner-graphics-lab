# B03-S03: RHI Device And Runtime Verification

## Verification Target

This packet independently verifies the fixes committed at `98c97a5`:

- `CR001-B03-F001`: non-wrapping live-object aggregation.
- `CR001-B03-F002`: request-consistent native execution proof.

No production or test implementation changed during verification.

## Parent Reproduction

The parent contract was extracted from commit `2856823`. Its Git blob ID,
`dabb110f9fbf59a21b7a950d0651489ea60eaf15`, exactly matched the temporary
header used for compilation.

The same strict C++20 verifier used for the current contract produced:

```text
total=0
contradictory_native_proof=1
classification=parent-defects
parent_exit=3
```

This reproduces both accepted findings without relying on the original
inspection probe or repository unit tests.

## Current Verification

Compiled against the current public RHI header, the same verifier produced:

```text
total=4294967296
contradictory_native_proof=0
classification=fixed
current_exit=0
```

The result proves both the widened aggregate and the deterministic-request
rejection at the public contract boundary.

## Downstream Compatibility

A repository-wide call-site audit found no assignment of
`GetTotalLiveObjectCount()` into `uint32`. Existing consumers compare the
result with constants or stream it into diagnostics, so the wider return type
does not introduce truncation.

The fresh `fallback-strict` gate passed its strict build and complete
graphics-disabled test run. The retained 757-line output contains the six new
runtime boundary assertions and no `[FAIL]` record.

The strict Release and ASan/UBSan gate records generated against the same
production fix remain passing. The sanitizer profile executes the full
non-optional suite and explicitly skips only the already-tracked intermittent
deferred-native case.

## Finding Status

- `CR001-B03-F001`: Verified.
- `CR001-B03-F002`: Verified.

The native deferred readback issue remains independently Accepted as
`CR001-B08-F001`; it neither invalidates nor broadens these runtime contract
findings.

## Next Packet

B03-S04 may inspect commands, queues, synchronization, and swapchain behavior.
It must independently assess the surface swapchain default adapter recorded as
a lead in B03-S01.
