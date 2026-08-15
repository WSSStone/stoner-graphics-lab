# Contract: Runtime Asset Manager API

## Construction

`FAssetManager::Create(Config, OutManager, Diagnostics)` validates the complete
immutable configuration before creating workers. Cooked mode additionally binds
one generation. Failure returns no partially usable manager.

Required public vocabulary:

- `EAssetManagerMode { DevelopmentSource, StrictCooked }`
- `EAssetRequestState { Invalid, Accepted, WaitingForDependencies, Loading,
  Ready, Failed, Cancelled }`
- `FAssetManagerConfig`, `FAssetManagerLimits`
- `FAssetRequestHandle`, `FAssetRequestSnapshot`
- `FAssetRequestResult`, `FAssetManagerInspection`
- `TAssetHandle<T>`

No public type contains native handles, threads, paths to temporary storage,
Tools, Renderer, RHI, Backend, or graphics API types.

## Request

```cpp
template <CTypedAssetPayload T>
EAssetResult Request(
    const FAssetId& Id,
    FAssetRequestHandle& OutRequest,
    FAssetCompletionCallback Completion = {});
```

- `TAssetTypeTraits<T>::GetAssetType()` must equal `Id.GetAssetType()`.
- Success means admission, not load completion.
- Capacity/configuration/shutdown rejection leaves `OutRequest` invalid.
- A callback-bearing request reserves one bounded completion slot atomically
  with admission. If no slot is available, admission fails and creates no
  request or operation interest.
- Equal complete load keys may coalesce, but each call receives a distinct
  request handle and cancellation interest.

## Observe And Retrieve

```cpp
EAssetResult Query(FAssetRequestHandle, FAssetRequestSnapshot&) const;

template <CTypedAssetPayload T>
EAssetResult GetResult(FAssetRequestHandle, TAssetHandle<T>&) const;
```

- Query is non-blocking, idempotent, and executes no callback.
- `GetResult` succeeds only for Ready and returns another immutable retention.
- Failed/Cancelled returns its stable terminal category; non-terminal returns
  `NotReady`; stale/foreign returns `InvalidHandle`.
- A successful typed handle remains valid after request release, cache removal,
  manager shutdown, and manager destruction.

## Cancel And Release

```cpp
EAssetResult Cancel(FAssetRequestHandle);
EAssetResult ReleaseRequest(FAssetRequestHandle);
```

Cancel is idempotent for a valid caller interest. It cannot revoke Ready and
cannot cancel shared work still retained elsewhere. Release drops observation
and undelivered-result interest; it does not invalidate typed handles already
obtained. Reusing a slot increments generation.

## Completion Pump

```cpp
FAssetPumpResult PumpCompletions(Core::uint32 MaxCount);
```

- `MaxCount > 0`; dispatch order is terminal enqueue sequence.
- Callbacks run only on the calling thread and never under manager locks.
- Query, GetResult, Cancel, ReleaseRequest, and Request are callback-safe.
- Recursive Pump on the same manager returns `ReentrantPump` and dispatches
  nothing in the recursive call.
- Dispatch or request-interest release consumes its reserved completion slot
  exactly once; a terminal commit therefore never needs fallible queue growth.
- Callback exceptions are not supported by project build policy; callbacks must
  not throw.

## Shutdown

```cpp
EAssetResult Shutdown();
```

Shutdown is idempotent. It signals the cooperative token and monotonic deadline
provided to every runtime-compatible extension call, rejects new requests,
makes every accepted request terminal, joins workers, releases manager-owned cache and generation ownership,
and preserves published typed handles. Explicit shutdown leaves already queued
callbacks available to pump; destruction discards unpumped callbacks without
calling user code.

Runtime-compatible extensions must observe cancellation at bounded work
boundaries and return by their declared deadline. The manager neither detaches
nor forcibly terminates extension code; a never-returning third-party extension
violates the runtime extension contract.

## Inspection

`Inspect()` returns a bounded immutable snapshot ordered by AssetId/load key and
request identity. It includes mode, bound generation, counts by state,
coalescing/cache decisions, three retention classes, limits, and stable
diagnostics. Normalized output excludes addresses, raw handles, absolute
temporary paths, timestamps, and thread IDs.
