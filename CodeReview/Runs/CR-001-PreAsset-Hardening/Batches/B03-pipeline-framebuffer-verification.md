# B03-S12: Pipeline And Framebuffer Contract Verification

## Verification Target

This packet independently verifies the repairs committed at `09d1a1b`:

- `CR001-B03-F009`: shader and pipeline-layout declaration and compatibility
  validation;
- `CR001-B03-F010`: graphics fixed-function and render-pass closed domains;
- `CR001-B03-F011`: framebuffer mip, array-layer, and selected-extent
  semantics.

No production or maintained test implementation changed during verification.

## Defect Sensitivity

One strict C++20 verifier was compiled twice:

1. against exact fix parent `f7aa909`;
2. against current production and maintained test source at `fa6c536`.

The verifier uses only public factory contracts that existed in the parent.
It does not call any validation helper introduced by the fix.

The parent reported:

- 0 of 7 repaired shader/interface rejections;
- 0 of 12 repaired fixed-function/render-pass rejections;
- 0 of 3 repaired framebuffer subresource rejections;
- rejection of a valid selected mip.

Current source reported every repaired rejection, accepted the valid selected
mip, and preserved all six positive control paths. The same current verifier
also passed under ASan/UBSan.

## Export And Diff Integrity

- The parent source was exported directly with `git archive` from the full
  parent SHA.
- `git show --check 09d1a1b` passed.
- The fix commit contains exactly 13 production headers/sources and two
  maintained test sources.
- `git diff --quiet 09d1a1b..HEAD -- Source Tests` returned zero at
  verification time.

## Contract And Call-Site Review

- Mock graphics and compute factories and `FVulkanPipelineLayout` use the
  shared shader-interface compatibility rule.
- Mock and Vulkan render-pass factories use the shared portable render-pass
  validator.
- Mock and Vulkan framebuffer factories use selected-mip extents and explicit
  mip/layer bounds.
- Undefined RHI enum construction is confined to maintained negative tests.
  Production casts in `ERHIShaderStage.h` are named stage-to-flag conversion,
  not undefined input construction.
- No second production validation policy was found for the repaired cases.

## Fresh Gate Evidence

The verification packet reran `fallback-strict` from current source:

- strict graphics-disabled Debug build with `-Werror`: passed;
- complete deterministic test executable: passed;
- 809 pass records and no failure record;
- RHI core summary: 211 passed, 0 failed;
- repaired mock and deterministic Vulkan assertions: passed.

The fix packet's native-capable strict Debug, strict Release, and ASan/UBSan
records remain applicable because no `Source` or `Tests` file changed after
`09d1a1b`.

Detailed commands and outputs are retained in
`Evidence/b03-pipeline-framebuffer-verification-probes.md`.

## Hosted Batch Gate

The completed B03 evidence commit `4ce090e` was pushed once at the batch
boundary. GitHub Actions ran on exact head
`4ce090efc430e501081f19a0869b623084ff2b1f`:

- CI run `30200859411`: success;
- Code Review Tools run `30200859409`: success;
- Linux, macOS, and Windows headless jobs: success;
- Linux, macOS, and Windows strict Release jobs: success;
- Linux ASan + UBSan: success;
- CR CLI unit tests and CR-001 state validation: success.

Machine-readable run and check records are retained in:

- `Evidence/remote-ci-b03.json`;
- `Evidence/remote-tools-b03.json`;
- `Evidence/remote-pr-checks-b03.json`.

The evidence-only follow-up commit is intentionally held locally until the
next batch boundary so recording a successful workflow does not trigger a
redundant workflow by itself.

## Finding Decisions

- `CR001-B03-F009`: Verified.
- `CR001-B03-F010`: Verified.
- `CR001-B03-F011`: Verified.

This packet closes B03. Its evidence commit is pushed at the batch boundary,
then the existing cross-platform GitHub Actions checks are observed before
the CR proceeds to B04.
