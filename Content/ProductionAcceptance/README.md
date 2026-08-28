# Production Acceptance Content

This directory owns the bounded, immutable Feature 028 technical acceptance
corpus and reviewed image baselines.

- `Regular/` contains source bytes required by the regular clean-checkout gate.
- `Corpus/` contains canonical provenance, inventory, and coverage manifests.
- `Baselines/` contains reviewed backend/device-class records and losslessly
  compressed PNG references. Raw PPM/RGBA captures are never checked in.
- `MAINTAINER_NOTES.md` is out-of-band maintainer text. Validators must never
  open, parse, hash, inventory, or make an acceptance decision from it.

Externally staged medium content belongs under ignored
`Content/ProductionAcceptance/External/` or a caller-selected build cache.
DDC, cooked generations, raw logs, raw readbacks, and unreviewed captures are
generated state and must not be committed. They belong under ignored
`Build/Validation/` (or external CI artifact retention). When a reviewed image
must be retained, its PNG is accepted only after decoded RGB pixels are proven
identical to the reviewed raw capture and the registry records the PNG digest.

Verify the clean-checkout regular corpus without network access:

```text
python3 .github/scripts/verify_production_corpus.py --tier regular
```

Acquire and verify the optional pinned medium package, then verify both tiers:

```text
python3 .github/scripts/acquire_production_corpus.py \
  --package khronos-sponza-gltf
python3 .github/scripts/verify_production_corpus.py --tier all
```

Admission maintainers can regenerate the reviewed inventory from a checkout of
the exact upstream revision with `build_production_corpus_manifest.py`. Changes
to generated corpus JSON are reviewed like source; ordinary validation never
regenerates or updates accepted hashes.
