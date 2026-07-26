# B02-S04 Aligned Allocation Boundary Probe

## Environment

- Host: macOS arm64
- Compiler: Apple Clang, C++20
- Reviewed head: `a7e6c2863d4ac66c720761637fa01d21eb2e4e86`

## Probe

The temporary probe called:

```cpp
void* Pointer = Stoner::Core::FMemory::AllocateAligned(
    std::numeric_limits<std::size_t>::max(),
    16);
```

It did not dereference the result. A non-null result was released with
`FMemory::DeallocateAligned`.

## Result

```text
non-null
probe_exit=1
```

The temporary source and executable remained under `/tmp` and are not part of
the repository.
