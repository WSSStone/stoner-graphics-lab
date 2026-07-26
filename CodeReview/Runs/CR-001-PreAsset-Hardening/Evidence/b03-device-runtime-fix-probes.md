# B03 Device And Runtime Fix Evidence

## Contract Probe

The standalone probe is compiled as C++20 with
`-Wall -Wextra -Werror` against the public Core and RHI headers.

- Source SHA-256:
  `b5996df8341f394a8a53450c61f236a88f5e7b5334cd2367cf2b04b26b772c7f`
- Binary SHA-256:
  `eb671737889821f4ff1f7edb8563b3cc4553bc0c038a016c985a134535197dca`

Command:

```sh
clang++ -std=c++20 -Wall -Wextra -Werror \
  -ISource/Core/Public -ISource/RHI/Public \
  /tmp/cr001_b03_runtime_snapshot_fix_probe.cpp \
  Build/Mac/Debug/Core/libCore.a \
  -o /tmp/cr001_b03_runtime_snapshot_fix_probe

/tmp/cr001_b03_runtime_snapshot_fix_probe
```

Output and exit status:

```text
aggregate=4294967296
native=1
native_headless=1
deterministic_real=0
fallback=0
missing_instance=0
missing_device=0
exit=0
```

The probe also statically asserts that the aggregate return type is
`Core::uint64`.

## Repository Test Matrix

The strict graphics-disabled build used:

```sh
conda run -n stoner-cr scons \
  config=debug strict=1 graphics=disabled \
  Build/Mac/Debug/Tests/StonerTest
```

The authorized test run exited zero. Its 757-line output contained the six new
boundary records:

```text
[PASS] RHI runtime snapshot aggregates live counts without wrapping to zero
[PASS] RHI runtime snapshot accepts native-headless execution proof
[PASS] RHI runtime snapshot rejects native proof for a deterministic request
[PASS] RHI runtime snapshot rejects deterministic fallback as native proof
[PASS] RHI runtime snapshot requires a live native instance
[PASS] RHI runtime snapshot requires a live native device
```

Full log SHA-256:
`41bee991671858cca259811917929f0ea62e439931295d8d93dfece4cc01de3b`

## Predefined Gate Records

- Strict Debug JSON SHA-256:
  `c4e61f66799d200f42cd990c6fe67fa2517d6933fc002939b1ed7604eb037084`
- Strict Release JSON SHA-256:
  `2546cbecd135a4d19b0557a3d91d40db7de2e2d3784cfffd65bbfc9bb077ddc1`
- ASan/UBSan JSON SHA-256:
  `bbe6b4711fc164b36da0c6efc45e2be83eeed4c4dddc07e38f52fe0de01d2c91`

All three predefined records report `passed: true`.

## Disclosed Native Result

Two authorized native-enabled runs reached all RHI runtime tests and passed
the deterministic triangle lifecycle. Both then failed only:

```text
[FAIL] Deferred native validation completes a real Vulkan submission
[FAIL] Mapped attachment probes are finite, unique, and within semantic tolerances
[FAIL] Deferred native validation passes semantic probes and releases frame-owned objects
```

The repeated full log SHA-256 was:
`e0a65c4d6dc9faf3a8bd37d367f06f1953b09b14aee9cedf22a68714eea56ff1`.
This exactly matches the scope and evidence of existing Accepted finding
`CR001-B08-F001`; it is retained for the B08 integration packet and is not
silently waived.
