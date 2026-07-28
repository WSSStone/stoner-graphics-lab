# Contract: Core Unicode Normalization API

**Feature**: 020-asset-core
**Public header**: `Source/Core/Public/Core/FUnicode.h`

## Purpose

Provide one cross-platform Core boundary for validating UTF-8 and normalizing it
to Unicode NFC. Asset identity uses this boundary without depending directly on
the private Unicode implementation.

## Public Vocabulary

```cpp
namespace Stoner::Core
{

enum class EUnicodeResult : uint8
{
    Success,
    InvalidUtf8,
    ConversionFailed
};

struct FUnicode
{
    [[nodiscard]] static EUnicodeResult NormalizeNFC(
        const FString& Input,
        FString& OutNormalized) noexcept;
};

} // namespace Stoner::Core
```

The exact declaration may add `const`-correct helper overloads during
implementation, but it must preserve these result and ownership semantics.

## Behavior

- Valid UTF-8 returns `Success` and NFC text.
- Canonically equivalent valid inputs return byte-identical output.
- Already-NFC input returns byte-identical text.
- ASCII remains unchanged.
- Empty input is valid and returns empty output; Asset identity separately
  rejects empty required components.
- Invalid, truncated, overlong, surrogate, or out-of-range UTF-8 returns
  `InvalidUtf8`.
- Allocation or private conversion failure returns `ConversionFailed`.
- On every failure, `OutNormalized` is empty.
- The function does not case-fold, compatibility-normalize, strip marks, change
  path separators, or apply Asset path grammar.
- Repeated calls are thread-safe and use no mutable process-global locale.

## Private Implementation Boundary

- `utf8proc 2.11.3` and its Unicode 17 tables live under
  `ThirdParty/utf8proc`.
- Only `Source/Core/Private/FUnicode.cpp` and Core build wiring may include
  `utf8proc.h`.
- `Core/FUnicode.h`, `CoreMinimal.h`, Asset public/private code, and downstream
  layers expose no `utf8proc` type, macro, allocator, or header.
- Project-owned wrapper code remains under strict compiler warnings.
- Third-party source is compiled with isolated warning policy and retains its
  upstream license and version record.

## Verification

- Published composed/decomposed pairs normalize identically.
- At least 20 pairs used by Asset identity pass on Windows, macOS, and Linux.
- Representative invalid UTF-8 classes fail and clear output.
- Re-normalizing output is idempotent.
- Concurrent normalization calls return stable results.
- The pinned upstream version and Unicode data version are inspectable in build
  or test diagnostics.

