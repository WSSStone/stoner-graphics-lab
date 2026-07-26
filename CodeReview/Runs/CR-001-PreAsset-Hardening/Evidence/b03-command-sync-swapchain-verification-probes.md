# B03-S06 Parent And Current Verification Evidence

## Verified Revisions

- Fix commit: `b29f466cf81eeaa90c94a906b89675e5ba23f4aa`
- Exact parent: `0e1ccf1386049c5b1d3469e08a0c768c30dab525`
- Verification HEAD: `2dad59af87e64fd851162d0340d3ae8258d44fd1`

`git diff --quiet b29f466..HEAD -- Source Tests` returned zero, proving
verification HEAD carries exactly the fix commit's production and maintained
test state.

## Parent Export Identity

The parent `Source/Core/Public`, `Source/RHI/Public`, and RHI core test source
were exported with `git archive`. The extracted files used for compilation
match their parent Git blobs exactly:

```text
IRHIDevice.h:
0344851a86fd5d01df72f778404ae63a92b6a4d6
0344851a86fd5d01df72f778404ae63a92b6a4d6

IRHICommandBuffer.h:
a8c713c942cd14892ebe8c8b87a44111d0c5d0e0
a8c713c942cd14892ebe8c8b87a44111d0c5d0e0

IRHISwapchain.h:
41bec92caac2b9d74a079721d0c1f78ace61dda3
41bec92caac2b9d74a079721d0c1f78ace61dda3

RHICoreTests.cpp:
2bc8440f3c407b742ab0e27b42c8d89bc1c796af
2bc8440f3c407b742ab0e27b42c8d89bc1c796af
```

Each pair is `git rev-parse parent:path` followed by `git hash-object` of the
compiled export.

The initial export omitted `Tests/RHICoreTests.h`, so preprocessing stopped at
the missing include before compiling the verifier. The exact parent header was
then exported from the same commit; the successful evidence below uses only
that complete parent snapshot.

## Same-Source Verifier

Source:

`Evidence/Probes/b03-command-sync-swapchain-verification-probe.cpp`

The same source tests seven independent predicates:

1. surface-aware swapchain creation fails closed;
2. explicit clear compatibility fails closed;
3. synchronized swapchain compatibility fails closed;
4. failed acquire is state-atomic;
5. failed present is state-atomic;
6. failed wait preflight is state-atomic;
7. failed signal preflight is state-atomic.

### Current Build

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. -ISource/Core/Public -ISource/RHI/Public -ITests \
  Evidence/Probes/b03-command-sync-swapchain-verification-probe.cpp \
  -o /tmp/cr001_b03_s06_current_probe

/tmp/cr001_b03_s06_current_probe current
```

Output:

```text
surface_fixed=1
clear_fixed=1
swapchain_fallback_fixed=1
acquire_atomic=1
present_atomic=1
queue_wait_atomic=1
queue_signal_atomic=1
fixed_count=7
classification=current-fixed
```

### Parent Build

The same source was compiled with include roots pointing only at the exact
parent export:

```text
/tmp/cr001_b03_s06_parent_probe parent
```

Output:

```text
surface_fixed=0
clear_fixed=0
swapchain_fallback_fixed=0
acquire_atomic=0
present_atomic=0
queue_wait_atomic=0
queue_signal_atomic=0
fixed_count=0
classification=parent-defects
```

Both invocations exit zero only for their expected classification.

## Maintained Gate Recheck

A fresh `fallback-strict` gate at verification HEAD passed:

- strict graphics-disabled Debug build with `-Werror`;
- complete deterministic test executable;
- 770 result lines;
- no `[FAIL]` record;
- all 10 B03-S05 focused assertions passed.

The fix packet's current-code `strict-debug`, `strict-release`, and
`sanitizers` gate records also remain passing. No production or test source
changed between those gates and verification HEAD.

