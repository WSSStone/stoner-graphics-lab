# B07-S11 Evidence: Hierarchy, Reparenting, And Destruction Fix

Step: `B07-S11`.

Finding fixed:

- `CR001-B07-F006`: Transform propagation stops at transformless intermediate
  parents.

Resolution:

- `ComputeHierarchyWorldTransform` computes parent-chain world transforms for
  internal hierarchy propagation and treats transformless intermediate nodes as
  identity local transforms that inherit ancestors.
- `ComputeWorldTransform` still returns false when the queried entity itself
  has no transform, preserving render collection's missing-transform rejection
  behavior.
- `SetParent` uses hierarchy-world parent transforms when evaluating
  preserve-world/default and preserve-local feasibility.

Regression evidence:

- `[PASS] Scene transformless hierarchy groups inherit ancestor world transforms`
- `[PASS] Scene preserve-world reparent works through transformless hierarchy groups`
- `[PASS] Scene render collection diagnostics and dumps are byte-stable across repeated runs`

Gate evidence:

- `git diff --check -- Source/Application/Public/Application/FWorld.h Source/Application/Private/FWorld.cpp Tests/ApplicationSceneEcsTests.cpp`: passed.
- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed, with output captured in
  `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b07-hierarchy-fix-stonertest.txt`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T08:28:04+00:00`.

Code commit:

- `00d3ffc`: `fix(application): propagate transforms through hierarchy groups`
