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

## Pending Cross-Platform Evidence

Windows/Linux strict builds, focused/full tests, Linux ASan/UBSan,
ThreadSanitizer, independent validation, digest comparison, and Lavapipe native
compressed upload/readback remain GitHub Actions gates. Feature 022 stays
In Progress until those jobs pass.
