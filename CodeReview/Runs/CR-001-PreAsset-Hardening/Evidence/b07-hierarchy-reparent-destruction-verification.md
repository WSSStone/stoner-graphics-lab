# B07-S12 Evidence: Hierarchy, Reparenting, And Destruction Verification

Step: `B07-S12`.

Finding verified:

- `CR001-B07-F006`: Transform propagation stops at transformless intermediate
  parents.

Static verification evidence:

- `Source/Application/Public/Application/FWorld.h:93` and
  `Source/Application/Public/Application/FWorld.h:95` declare the private
  hierarchy helpers.
- `Source/Application/Private/FWorld.cpp:348` to
  `Source/Application/Private/FWorld.cpp:356` use hierarchy-world parent
  availability for reparent preservation checks.
- `Source/Application/Private/FWorld.cpp:597` keeps public world transform
  queries strict for queried entities without transform components.
- `Source/Application/Private/FWorld.cpp:627` implements internal hierarchy
  world propagation through transformless intermediate nodes.

Regression evidence:

- `Tests/ApplicationSceneEcsTests.cpp:176` covers
  `Root(transform) -> Group(no transform) -> Child(transform)` propagation.
- `Tests/ApplicationSceneEcsTests.cpp:182` covers preserve-world reparenting
  through transformless groups under transformed ancestors.
- `Tests/ApplicationSceneEcsTests.cpp:297` continues to assert render
  collection missing-transform rejection, proving public query semantics did
  not broaden mesh acceptance.

Gate evidence:

- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T08:31:12+00:00`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b07-hierarchy-fix-stonertest.txt`
  records the new hierarchy regressions as passing.

Result:

- `CR001-B07-F006` is verified.
