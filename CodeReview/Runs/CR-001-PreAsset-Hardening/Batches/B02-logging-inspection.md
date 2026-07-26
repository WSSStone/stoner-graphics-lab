# B02-S13: Logging System Inspection

## Inspection Budget

The inspection covered one logging responsibility domain and eight production
files, totaling 450 production lines:

1. `Source/Core/Public/Core/ELogSeverity.h`
2. `Source/Core/Public/Core/FLogCategory.h`
3. `Source/Core/Private/FLogCategory.cpp`
4. `Source/Core/Public/Core/FLog.h`
5. `Source/Core/Private/FLog.cpp`
6. `Source/Core/Public/Core/FLogConsoleSink.h`
7. `Source/Core/Private/FLogConsoleSink.cpp`
8. `Source/Core/Public/Core/SGLog.h`

Supporting evidence included Feature 005's specification, plan, research, data
model, API contract, quickstart, tasks, logging tests, call sites, implementation
history, standalone Debug/Release probes, and ThreadSanitizer probes. No
production implementation was changed.

## Requirement Mapping

- `005-FR-002`, `FR-003`, `FR-005`, and `FR-006`: category/global filtering
  APIs exist, but the global threshold is not part of the macro early-out and
  both runtime-mutable threshold states are unsynchronized.
- `005-FR-004` and `FR-012`: the implementation routes Fatal to `stderr` and
  aborts, but the repository test suite does not execute this contract.
- `005-FR-013`: sink writes are serialized by `GLogMutex`; the inspected
  threshold configuration paths are outside that synchronization.
- `005-FR-016`: category filtering short-circuits at the macro, while global
  filtering occurs only after argument evaluation and entry into
  `FLog::LogMessage`.
- `005-SC-009`, T011, T013, and T048: checked-complete coverage claims are not
  supported because Fatal routing and termination are deliberately skipped.
- Feature 005 research requires an atomic category threshold read by the macro;
  the implementation instead uses plain `ELogSeverity` objects for both
  thresholds.

## Reproduction

Standalone Debug-like and optimized Release probes set the global threshold to
Warning, the category threshold to Verbose, and issue an Info log with
`++SideEffectCount`:

```text
side_effect_count=1
```

Both probes exited `1`, proving the globally suppressed message evaluates its
argument. Independent macOS ThreadSanitizer probes reported data races for:

```text
FLog::SetGlobalMinSeverity <-> FLog::GetGlobalMinSeverity
FLogCategory::SetMinSeverity <-> FLogCategory::GetMinSeverity
```

An optimized Release child probe emitted the expected Fatal line to `stderr`
and terminated with exit `134`. This confirms the implementation behavior and
isolates the finding to missing durable test evidence.

## Findings

### CR001-B02-F010 - Accepted S2

`SG_LOG` compares only the category threshold. A message suppressed by the
global threshold still evaluates all macro arguments and calls
`FLog::LogMessage`, directly violating FR-016's observable side-effect and
cost contract.

### CR001-B02-F011 - Accepted S2

`FLogCategory::MinSeverity` and `GGlobalMinSeverity` are plain mutable enum
objects. Concurrent runtime reconfiguration and filtering performs conflicting
unsynchronized accesses, and ThreadSanitizer reproduces both data races.

### CR001-B02-F012 - Accepted S2

The severity-routing loop explicitly skips Fatal, while
`TestFatalLogBehavior` emits an Error message labeled as Fatal-like. T013's
suggested assertion-handler interception cannot prevent `FLog::LogMessage`
from aborting, so a child-process test is required to verify routing and
termination without killing the test runner.

## Confirmed Strengths

- Severity ordering and string labels match the public contract.
- Normal-size messages have the specified timestamp, category, severity, and
  message layout.
- The console sink routes Verbose/Info to `stdout` and Warning/Error/Fatal to
  `stderr`.
- A mutex protects each complete sink write from character-level interleaving.
- Fatal implementation logs before aborting, with the Debug break isolated by
  the platform abstraction.
- The inspected implementation remains inside Core and has no upward engine
  dependency.

## Observations

- Static category lifetime is an explicit data-model assumption, so the raw
  registry pointers are not raised as a defect in this packet. The public
  constructor still does not enforce the documented non-null, non-empty name
  rule.
- `FLogCategory.h` contains an immediately undefined duplicate macro pair, and
  `SG_LOG` uses the extension spelling `##__VA_ARGS__` instead of C++20
  `__VA_OPT__`. These are local S3 cleanup candidates, not independent
  accepted findings.

## B02-S14 Fix Packet

The next packet may repair at most these three related findings:

1. Make macro-level filtering account for the effective category/global
   threshold without evaluating suppressed arguments, and add side-effect
   regressions.
2. Make category and global runtime thresholds race-free while preserving the
   cheap read path, with concurrency coverage.
3. Add a portable isolated Fatal child mode that proves `stderr` routing and
   abnormal process termination across supported platforms.
