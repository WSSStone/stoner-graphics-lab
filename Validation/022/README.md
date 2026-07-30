# Feature 022 Validation Evidence

Feature 022 implements deterministic KTX2 cooking, bounded load/transcode,
compressed RHI realization, and Vulkan compressed texture support.

## Local macOS Evidence

The following gates passed on 2026-07-29:

- strict Debug and strict Release builds;
- complete Debug regression;
- Asset, Renderer texture, RHI, and Vulkan fallback suites;
- Asset architecture, third-party provenance, and validator adapter checks;
- 20 repeated cooks of all 19 artifacts with byte-identical results;
- independent KTX 4.4.2 validation of 19 generated plus 19 golden artifacts.

`macOS/determinism.json` contains the normalized artifact, quality, and size
report. `macOS/ktx-validate.json` contains the independent Khronos validator
report. The validator ran with warnings-as-errors and accepted only the exact
nine-key custom metadata diagnostic set documented by Feature 022.

The local M4 Pro host reports MoltenVK/Metal unavailable, so its native Vulkan
suite records the explicit unavailable branch. This is not counted as native
compressed-format success.

## Cross-Platform CI Evidence

GitHub Actions run
[30509436643](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/30509436643)
passed on 2026-07-30 for merge commit
`e8d430dde5e17253374af889eb84dd7255e987b3`.

The successful matrix covered:

- Windows, macOS, and Linux headless Debug builds, focused suites, full
  regression, independent validation, and 20-run digest comparison;
- Windows, macOS, and Linux strict Release builds;
- Linux ASan/UBSan focused and full validation;
- Linux ThreadSanitizer Asset concurrency validation;
- conditional Linux Lavapipe compressed upload/readback with capability and
  behavior agreement.

All required local and hosted gates are complete. Feature 022 is Done.
