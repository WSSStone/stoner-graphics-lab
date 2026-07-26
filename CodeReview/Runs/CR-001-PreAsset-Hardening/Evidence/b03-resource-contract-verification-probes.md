# B03-S09 Parent And Current Resource Verification Evidence

## Verified Revisions

- Fix commit: `ca68ed408146c2b7cf9c3d4192deff52256cfa8c`
- Exact parent: `d8c59b70de99d2272084131e4f7543fe66384411`
- Verification HEAD: `8e84b28f271e4d6d5f0f394bebd63ab183979aec`

`git diff --quiet ca68ed4..HEAD -- Source Tests` returned zero.

## Parent Export Identity

The exact parent was exported with `git archive`. Each compiled file's
`git hash-object` result matched the corresponding parent blob:

```text
96bf16cf7189cc283acb582f6600af34c98f2f1c  ERHIResourceUsage.h
3f40d5cb7003b49b3ad846a45c65aea3b2dc4e30  ERHISamplerMode.h
5091042acf66e239f58507d489a0abc8a55d5ff4  FRHIBufferDesc.h
e8b5ae5e8f7a1468536afb3030dd22afd4607403  FRHISamplerDesc.h
1b03dc36b05b72f31309ba3656eedba2ff3e83d5  FRHITextureDesc.h
5d364cbc77c56e88b3c86a8eb1c66de8b877e8c3  RHICoreTests.cpp
```

## Same-Source Verifier

Source:

`Evidence/Probes/b03-resource-contract-verification-probe.cpp`

### Parent Build And Run

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. -ISource/Core/Public -ISource/RHI/Public -ITests \
  /tmp/cr001-b03-s09-resource-verification-probe.cpp \
  -o /tmp/cr001_b03_s09_parent_verifier

/tmp/cr001_b03_s09_parent_verifier parent
```

Output:

```text
closed_domain_rejections=0/12
valid_mip_boundaries=3/3
invalid_mip_rejections=0/4
format_usage_rejections=0/2
classification=parent-defects
```

### Current Build And Run

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. -ISource/Core/Public -ISource/RHI/Public -ITests \
  Evidence/Probes/b03-resource-contract-verification-probe.cpp \
  -o /tmp/cr001_b03_s09_current_verifier

/tmp/cr001_b03_s09_current_verifier current
```

Output:

```text
closed_domain_rejections=12/12
valid_mip_boundaries=3/3
invalid_mip_rejections=4/4
format_usage_rejections=2/2
classification=current-fixed
```

Both invocations exit zero only for their expected revision classification.

## Call-Site Audit

Focused searches found:

- undefined resource/sampler casts and texture `Vertex` only in
  `Tests/RHICoreTests.cpp`;
- public validator consumers in the mock factory and
  `FVulkanDevice.cpp`;
- no second production validity policy for these repaired cases.

## Fresh Gate

`Evidence/gate-fallback-strict.json` records a fresh pass at
`2026-07-26T10:27:35Z`:

- strict graphics-disabled Debug build with `-Werror`;
- complete test executable returned zero;
- authorized repeat: 791 output lines, 777 pass records, no failure record;
- RHI core: 186 passed, 0 failed;
- 21 focused repaired-boundary assertions passed.

The strict Debug, strict Release, and sanitizer evidence from B03-S08 remains
valid because current `Source` and `Tests` content exactly matches the fix
commit.

## Sandbox Artifact Classification

A non-escalated repeat returned one demo lifecycle failure because the process
could not write
`Build/Mac/Debug/Tests/triangle-demo-test-report.txt` in the external
worktree. The same binary passed with the worktree permission used by
`crctl gate`; the report then recorded:

```text
completed-frames=20
validation-result=pass
```

No product finding was created from this sandbox-only denial.
