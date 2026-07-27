# B06-S13 Evidence: Forward Frame Planning, Lights, Views, And Sorting Inspection

Step: `B06-S13`.

Inspected forward frame planning, view validation, light selection, draw
validation, and transparent sorting against Feature 015 contracts.

Key evidence:

- `FForwardViewData::IsValid` and `FForwardOutputTarget::IsValid` cover required
  view/output finite and extent validation.
- `PrepareForwardLightSet` implements default/configurable point light selection
  using influence score, light id, and name.
- `SortForwardTransparentDraws` stops at depth, material id, and object id and
  has no final stable key for equal same-object draw commands.
- `FForwardRenderer::PrepareFrame` gates ambient fallback on
  `Configuration.bEnableAmbientFallback`, allowing valid no-light geometry plans
  without the required fallback diagnostic when disabled.

Findings:

- `CR001-B06-F004`: Transparent draw ordering can fall back to caller order for
  equal depth, material, and object keys. Severity S2, Accepted.
- `CR001-B06-F005`: Forward renderer can prepare no-light geometry without the
  required ambient fallback. Severity S2, Accepted.

No production or test source changed in this inspection step.
