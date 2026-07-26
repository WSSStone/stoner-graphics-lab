# B03-S12 Parent And Current Pipeline Contract Verification Evidence

## Verified Revisions

- Fix commit: `09d1a1b6d68393d05b7f75b0d3c3864463dc6757`
- Exact parent: `f7aa909bf4339480387290e4a0deae518f84cae6`
- Verification HEAD: `fa6c536a4870608d8c48cd5e7bcbae22eb46198f`

`git diff --quiet 09d1a1b..HEAD -- Source Tests` returned zero.
`git show --check 09d1a1b` also returned zero.

## Parent Export

The exact parent was exported into a fresh temporary directory:

```text
git archive --format=tar \
  --output=/tmp/cr001-b03-s12-parent.tar \
  f7aa909bf4339480387290e4a0deae518f84cae6

tar -xf /tmp/cr001-b03-s12-parent.tar \
  -C /tmp/cr001-b03-s12-parent.Ss60cE
```

The fix commit file boundary was recorded with:

```text
git diff-tree --no-commit-id --name-only -r 09d1a1b
```

It listed the 13 repaired RHI/Vulkan files and the two maintained test files,
with no CR document, build-system, feature-specification, or unrelated source
change.

## Same-Source Verifier

Source:

`Evidence/Probes/b03-pipeline-framebuffer-verification-probe.cpp`

The source includes the stable Feature 008 mock fixture and calls only parent
factory APIs. Parent and current builds used:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. -ITests -ISource/Core/Public -ISource/RHI/Public \
  /tmp/b03-pipeline-framebuffer-verification-probe.cpp \
  -o <revision-verifier>
```

### Parent Result

```text
interface_rejections=0/7
valid_interface_paths=2/2
state_rejections=0/12
valid_state_paths=2/2
invalid_subresource_rejections=0/3
baseline_subresource_acceptances=2/2
selected_mip_acceptances=0/1
classification=parent-defects
```

### Current Result

```text
interface_rejections=7/7
valid_interface_paths=2/2
state_rejections=12/12
valid_state_paths=2/2
invalid_subresource_rejections=3/3
baseline_subresource_acceptances=2/2
selected_mip_acceptances=1/1
classification=current-fixed
```

Both invocations returned zero only for their expected revision
classification. The current source was additionally compiled with:

```text
-fsanitize=address,undefined -fno-omit-frame-pointer \
-fno-sanitize-recover=all
```

That run returned zero with the same `current-fixed` output.

## Call-Site Audit

Focused searches found the repaired helpers in:

- the mock render-pass, graphics/compute, and framebuffer factories;
- `FVulkanPipelineLayout.cpp`;
- `FVulkanRenderPass.cpp`;
- `FVulkanFramebuffer.cpp`;
- maintained RHI and Vulkan regression tests.

Searches for undefined descriptor, visibility, format, fixed-function,
sample-count, attachment-role, load, and store casts found negative tests
only. No production bypass or duplicate competing policy was found.

## Fresh Gate

`Evidence/gate-fallback-strict.json` records a fresh pass at
`2026-07-26T11:44:48Z`:

- strict graphics-disabled Debug build with `-Werror`;
- complete deterministic test executable returned zero;
- authorized repeat: 809 pass records and no failure record;
- RHI core: 211 passed, 0 failed;
- repaired helper, mock factory, and deterministic Vulkan assertions passed.

The S11 strict Debug, strict Release, and sanitizer gate records remain valid
because `Source` and `Tests` exactly match fix commit `09d1a1b`.
