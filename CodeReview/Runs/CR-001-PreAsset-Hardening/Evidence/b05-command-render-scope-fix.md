# B05-S02 Local Evidence

## Implementation

- Commit: `7e92de1`
- Scope: command/render ownership, explicit factory failure, selected-mip
  transfer validation, and maintained regressions.
- Temporary diagnostic source: removed before commit.

## Strict Debug

Command:

```text
conda run -n stoner-cr scons config=debug strict=true sanitizers=none
```

Result: exit 0. Project sources compiled with `-Werror`.

## Full Debug Tests

Command:

```text
Build/Mac/Debug/Tests/StonerTest
```

Result: exit 0. The complete maintained suite passed, including:

- retained command-buffer invalidation after device destruction;
- zero-capacity command allocation rejection;
- selected-mip copy/readback validation;
- incompatible texture-copy format rejection;
- exact padded and overflow-safe readback footprint checks;
- deferred invalid-readback cleanup;
- existing native Vulkan and integration coverage available on this host.

## Strict Release

Command:

```text
conda run -n stoner-cr scons config=release strict=true sanitizers=none
```

Result: exit 0. Project sources compiled with `-Werror`.

## Repository Checks
