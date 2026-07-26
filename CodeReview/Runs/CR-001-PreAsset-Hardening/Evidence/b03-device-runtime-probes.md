# B03 Device And Runtime Probe Evidence

## Environment

- Host: macOS arm64
- Compiler: Apple clang 21.0.0 (`clang-2100.1.1.101`)
- Language profile: C++20 with `-Wall -Wextra -Werror`
- Probe source SHA-256:
  `4527ed27951e983acbfce7283f0ab9b91f9e9a4305203475c03e1b10e60096e9`
- Probe binary SHA-256:
  `ea6b9c6b0f02bf697ccda07a083f70ebe8e9e6c956e481b58f905908c94eb206`

The probe was intentionally kept outside the repository under `/tmp`; this
document retains the reproducible source and result while avoiding a
single-use tool in `CodeReview/Tools/`.

## Source

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

    const std::uint32_t WrappedTotal = Overflow.GetTotalLiveObjectCount();
    const bool bContradictoryProof = Contradictory.ProvesNativeExecution();

    std::cout << "wrapped_total=" << WrappedTotal << '\n';
    std::cout << "contradictory_native_proof="
              << (bContradictoryProof ? 1 : 0) << '\n';

    return WrappedTotal == 0 && bContradictoryProof ? 3 : 0;
}
```

## Command

```sh
clang++ -std=c++20 -Wall -Wextra -Werror \
  -ISource/Core/Public -ISource/RHI/Public \
  /tmp/cr001_b03_runtime_snapshot_probe.cpp \
  Build/Mac/Debug/Core/libCore.a \
  -o /tmp/cr001_b03_runtime_snapshot_probe

/tmp/cr001_b03_runtime_snapshot_probe
```

## Result

```text
wrapped_total=0
contradictory_native_proof=1
probe_exit=3
```

Exit code 3 means both defects were reproduced. The probe is expected to fail
to compile or return zero after B03-S02 changes the aggregate type and native
proof predicate; B03-S03 will replace this one-off probe with repository
regression tests.
