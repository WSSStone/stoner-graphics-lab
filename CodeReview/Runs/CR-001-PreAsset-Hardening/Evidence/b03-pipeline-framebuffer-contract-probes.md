# B03-S10 Shader, Pipeline, Render-Pass, And Framebuffer Probe Evidence

## Purpose

This strict standalone probe exercises public RHI declaration validators and
the Feature 008 mock factory with malformed values and subresources omitted by
maintained negative coverage. It does not modify production or maintained test
sources.

Source:

`Evidence/Probes/b03-pipeline-framebuffer-contract-probe.cpp`

## Command

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ITests \
  Evidence/Probes/b03-pipeline-framebuffer-contract-probe.cpp \
  -o /tmp/cr001_b03_s10_pipeline_framebuffer_probe

/tmp/cr001_b03_s10_pipeline_framebuffer_probe
```

## Observed Output

```text
interface_contract_defects=1
fixed_function_domains_accepted=1
framebuffer_subresource_defects=1
classification=pipeline-framebuffer-contract-defects
```

The probe exits zero only when all three current defect classes are
reproduced.

## Interface Declaration And Compatibility

The public layout validator and mock factory accept a binding whose descriptor
type is `255` and whose visibility is unnamed bit 31. They also accept two
overlapping constant ranges with the same compute-stage visibility.

A compute shader declares a 16-byte constant range, while its selected layout
declares no constant range. Both declarations and the resulting mock compute
pipeline are nevertheless successful. This contradicts Feature 012's explicit
requirement to validate shader interface metadata against pipeline layouts.
The Vulkan layout implementation performs this compatibility check, so the
omission also makes mock and backend contract behavior diverge.

## Fixed-Function And Render-Pass Domains

A graphics pipeline containing unknown cull mode, front face, blend factor,
depth compare operation, and matching unrecognized sample-count values passes
`IsValidRHIGraphicsPipelineState` and returns a usable mock pipeline.

A render-pass attachment with unknown role, sample count, load operation, and
store operation also returns a usable mock render pass when paired with a
depth format. The current mock and Vulkan validators interpret every non-color
role as depth-stencil instead of recognizing the closed attachment-role
domain.

## Framebuffer Subresource Selection

A two-layer, two-mip texture can be attached to the mock framebuffer with both
`ArrayLayer` and `MipLevel` equal to two, even though valid indices end at one.
Conversely, selecting valid mip one with the corresponding 32x32 framebuffer
extent is rejected because validation compares against the texture's 64x64
base extent.

The Vulkan framebuffer helper already checks layer and mip bounds, but it only
compares framebuffer dimensions to the base extent and does not require the
selected mip's extent. Backend repair remains limited to the following fix
packet; broader Vulkan object and native-view ownership stays in B04.

