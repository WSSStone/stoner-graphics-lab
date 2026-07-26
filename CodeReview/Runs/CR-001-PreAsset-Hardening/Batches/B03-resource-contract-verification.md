# B03-S09: Buffer, Texture, And Sampler Resource Verification

## Verification Target

This packet independently verifies the repairs committed at `ca68ed4`:

- `CR001-B03-F006`: undefined resource usage and enum domains;
- `CR001-B03-F007`: impossible texture mip chains;
- `CR001-B03-F008`: contradictory texture format/usage validity.

No production or maintained test implementation changed during verification.

## Defect Sensitivity

One strict C++20 verifier was compiled twice:

1. against the exact fix parent `d8c59b7`;
2. against the current production and maintained test tree.

The verifier separately counts:

- 12 closed-domain rejections;
- 3 valid mip boundaries;
- 4 invalid mip rejections;
- 2 color/depth format-and-usage rejections.

The parent preserved all three valid mip boundaries but reported zero repaired
rejections. Current code preserved the same valid boundaries and reported all
18 repaired rejections. This proves the verifier detects the original defects
without turning valid boundary cases into false negatives.

## Export And Diff Integrity

- Compiled parent copies of the five affected RHI headers and
  `Tests/RHICoreTests.cpp` match their Git blob IDs exactly.
- `git show --check ca68ed4` passed.
- The fix commit contains only those five public RHI headers and the RHI core
  maintained test source.
- `git diff --quiet ca68ed4..HEAD -- Source Tests` returned zero at
  verification time.

## Contract And Call-Site Review

- Repository search found no production construction of undefined usage,
  memory, sample, or sampler values; all such casts are confined to negative
  tests.
- Texture `Vertex` usage is likewise confined to its rejection test.
- The Vulkan device still delegates buffer, texture, and sampler portable
  validity to the repaired public helpers.
- Device-specific texture format support remains a separate capability check.
- The mock factory no longer maintains a contradictory private
  color/depth-attachment policy.

## Fresh Gate Evidence

The verification packet reran `fallback-strict` from current source:

- strict graphics-disabled Debug build: passed;
- complete deterministic test executable: passed;
- 777 pass records and no failure record;
- RHI core summary: 186 passed, 0 failed;
- all 21 new helper/factory boundary assertions passed.

An additional sandboxed invocation could not overwrite the demo report inside
the external CR worktree and therefore produced the expected report-failure
test result. Repeating the identical executable with the worktree permission
used by the gate passed and wrote a report with 20 completed frames and
`validation-result=pass`. This was an execution-permission artifact, not a
product finding.

The fix packet's strict Debug, strict Release, and ASan/UBSan records remain
applicable because no `Source` or `Tests` file has changed since `ca68ed4`.

Detailed commands, blob IDs, and outputs are retained in
`Evidence/b03-resource-contract-verification-probes.md`.

## Finding Decisions

- `CR001-B03-F006`: Verified.
- `CR001-B03-F007`: Verified.
- `CR001-B03-F008`: Verified.

The next packet is B03-S10, inspecting descriptor and pipeline-layout
contracts. No push or GitHub Actions run occurred in this packet.
