# B03 Device And Runtime Independent Verification Evidence

## Source Integrity

- Fix commit: `98c97a5`
- Parent commit: `2856823`
- Expected parent header blob:
  `dabb110f9fbf59a21b7a950d0651489ea60eaf15`
- Extracted parent header blob:
  `dabb110f9fbf59a21b7a950d0651489ea60eaf15`
- Verifier source SHA-256:
  `89d897d9552b6080063100d87d8e7571cd06e7f894373deddb14ae266a4a7984`
- Parent verifier SHA-256:
  `4dd6f396ae3c8404d2deb494d070e79c603ea4e9c8fad634d7ee581bd1c6fee1`
- Current verifier SHA-256:
  `86aac5ad37a6352f4c1d5120d648f3308df23e5087d16d563511c756eb98de4c`

## Independent Verifier

```cpp
#include "RHI/FRHIRuntimeSnapshot.h"

#include <cstdint>
#include <iostream>
#include <limits>

int main()
{
    using namespace Stoner::RHI;

    FRHIRuntimeSnapshot Overflow;
    Overflow.LiveInstances = std::numeric_limits<std::uint32_t>::max();
    Overflow.LiveDevices = 1;

    FRHIRuntimeSnapshot Contradictory;
    Contradictory.RequestedMode = ERHIRuntimeMode::Deterministic;
    Contradictory.ObjectMode = ERHIRuntimeObjectMode::RealRuntime;
    Contradictory.LiveInstances = 1;
    Contradictory.LiveDevices = 1;

    const auto Total = Overflow.GetTotalLiveObjectCount();
    const bool bNativeProof = Contradictory.ProvesNativeExecution();
    const bool bParentDefects = Total == 0 && bNativeProof;
    const bool bCurrentFixed =
        static_cast<Stoner::Core::uint64>(Total) ==
            (static_cast<Stoner::Core::uint64>(
                std::numeric_limits<std::uint32_t>::max()) + 1) &&
        !bNativeProof;

    std::cout << "total=" << Total << '\n';
    std::cout << "contradictory_native_proof=" << bNativeProof << '\n';
    std::cout << "classification="
              << (bCurrentFixed ? "fixed"
                  : bParentDefects ? "parent-defects" : "unexpected")
              << '\n';

    return bCurrentFixed ? 0 : bParentDefects ? 3 : 2;
}
```

Both builds used C++20 with `-Wall -Wextra -Werror`. The parent compile placed
the exact parent header directory before current include roots; the current
compile used repository include roots only.

## Results

Parent:

```text
total=0
contradictory_native_proof=1
classification=parent-defects
parent_exit=3
```

Current:

```text
total=4294967296
contradictory_native_proof=0
classification=fixed
current_exit=0
```

## Gate Evidence

- Fresh fallback-strict JSON SHA-256:
  `1eeaf18ee60c96e99e16da97ac1fb503586b5c082ea8e2cdd743238afc8a3bbb`
- Strict Release JSON SHA-256:
  `2546cbecd135a4d19b0557a3d91d40db7de2e2d3784cfffd65bbfc9bb077ddc1`
- ASan/UBSan JSON SHA-256:
  `bbe6b4711fc164b36da0c6efc45e2be83eeed4c4dddc07e38f52fe0de01d2c91`
- Retained fallback output SHA-256:
  `41bee991671858cca259811917929f0ea62e439931295d8d93dfece4cc01de3b`

All three gate records report `passed: true`. The retained fallback output has
757 lines, includes every new runtime boundary assertion, and contains no
`[FAIL]` record.
