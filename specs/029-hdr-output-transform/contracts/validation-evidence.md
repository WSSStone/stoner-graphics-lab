# Contract: Validation and Evidence

## Validation Tiers

| Tier | Name | Authority |
|---|---|---|
| L0 | `deterministic-contract` | Cross-platform math, settings, graph, profile, failure, diagnostics |
| L1 | `native-nonvisual` | Applicable real command completion/readback/presentation and resolved native state; no HDR appearance claim |
| L2 | `sdr-image-authority` | Exact 512x512 v3 PNG/semantic/FLIP Candidate with explicit maintainer acceptance |
| L3 | `macos-hdr-live-human-authority` | Physical M4 Metal live visual pass/fail for four profiles only |
| L4 | `closeout` | Aggregates exact lower-tier records without promoting or inferring them |

Windows participates in L0 and applicable SDR L1/L2, but no Feature 029 HDR
native/visual/physical authority claim. Linux Lavapipe is real Vulkan offscreen
L1 for applicable SDR/readback, not HDR display authority.

## SDR v3 Authority

- Feature 028 v2 remains immutable and selectable only by its old exact keys.
- Feature 029 uses a new v3 schema keyed by workload revision, backend, device
  class, profile, transform version, exposure, and settings digest.
- Formal images are exactly 512x512, sampleCount=1, lossless PNG.
- Dimension mismatch fails before FLIP. Translation, crop, scale, warp,
  resampling, automatic alignment, or best-match search is forbidden.
- Candidate generation is automated; acceptance is an explicit maintainer
  review action. Fresh M4 Metal and Windows Vulkan evidence is required.
- Windows T102 permits an active Console or RDP session on physical discrete
  Vulkan hardware. Candidate pixels must originate in the application's GPU
  readback, and native application-window presentation must still complete for
  the linked frame. Record the actual session, native adapter, and policy-diff
  identity beside the bounded bundle. RDP-client screenshots/video are excluded.
  RDP evidence covers that session's GPU output and presentation; it does not
  establish physical-monitor scanout, display calibration, client video fidelity,
  or Console/RDP equivalence. The macOS physical/live-view requirements remain.
- Each required Lantern/Sponza × Metal/Vulkan tuple needs exactly one
  `sdr-image-authority` report at the target software `gitRevision`. Its four
  SHA-checked artifacts are the immutable source Candidate JSON, exact PNG,
  cross-process calibration JSON, and same-frame native probe JSON. The report
  checks PNG bytes against the native RGBA/BGRA readback, frozen v3 settings,
  calibration revision/processes/mutations/policy, and Accepted-to-Candidate
  identity. Only `state`, `acceptance`, and the repository-relative
  `referencePath` change on admission; the Candidate itself stays immutable.

## Exact Software Revision

Formal producers require an exact 40-hex checkout HEAD and clean implementation,
shader, configuration, build, test, and validation-script inputs. They check
before and after capture. `native-capture` launches a JSON argv command without
a shell and requires a fresh probe path; offline relabeling of old probes is not
an authority-producing operation. SDR v3 calibration requires `--git-revision`
and records that revision; Feature 028 v2 calibration remains unchanged.

Rebuild executables and recook the platform closure at that frozen revision.
The guard is not a cryptographic proof of execution or human review. Reviewers
must check the build/cook provenance as well. Evidence-only and documentation
commits can follow the tested software commit without changing its identity.
Documentation-only session-policy changes are identified separately by their
exact diff/digest while all executable validation inputs retain the frozen SHA.
Never relabel a previous run as a newer commit. Changes to software inputs
require a new frozen revision and fresh applicable cross-platform evidence.
Unrelated tutorial edits and the explicit Accepted registry edit are excluded
from the software-input dirty check, not silently included in a feature commit.

## HDR Two-File Handshake

Automation may create only `hdr-live-review-request.json` with state
`not-run`, `unsupported`, `failed`, or `ready-for-live-review`. It cannot write
a visual decision. After live viewing, the maintainer authors a separate
immutable file under `Validation/029/HDR/Attestations/`, tied to the request
digest and all four ordered profiles.

The runner MUST NOT expose an accept option, environment variable, API, prompt
default, or attestation writer. Schemas reject perceptual score, threshold,
reference/candidate image, automatic decision, measured peak, PNG, video, or
desktop-capture fields. A schema-valid attestation is still a maintainer claim;
schema validation does not prove the human act. Code review and immutable digest
linkage supply provenance. Corrections append a superseding record.

## Aggregation

- Missing manual review is `manual-review-required`; failure is
  `human-attested-fail`; neither can become pass through automated evidence.
- `human-attested-fail` is immutable evidence and blocks L4 closeout. A corrected
  run appends a linked superseding attestation; L4 requires current
  non-superseded `pass` observations for all four HDR profiles.
- L4 may quote only a matching, non-superseded attestation; it never computes an
  overall HDR visual decision from native state.
- `Unsupported`, absent physical authority, digest mismatch, malformed schema,
  wrong revision/profile/frame token, or a Windows HDR claim fails/incompletes
  the appropriate tier.
- Producer artifacts are immutable. Consumers revalidate canonical JSON,
  schema, manifest, SHA-256, revision, profile, and provenance.
- Empty or partial machine reports cannot close out. Require all four physical
  SDR reports and four ordered Metal HDR native reports at the target revision.
  Every HDR report has one SHA-checked native probe artifact. Hash the ordered
  canonical report array for `nativeReportDigest` and ordered canonical raw probe
  array for `preflightDigest`; match profile, device, workload, mode, settled
  frame, readback, and adaptation to the request. The request and attestation
  must also match the target revision. A self-consistent pair for another
  revision cannot be quoted as current authority.
- Both the aggregate CLI and verifier `--require-closeout` enforce these gates.
  A schema-only verifier success is not closeout. Hosted matrix/sanitizer run
  IDs and digests at the frozen revision remain the separate T112 gate; a
  machine-workflow aggregate does not replace physical or human authority.

## Bounds and Privacy

Canonical JSON <=1 MiB; <=64 artifacts; each <=64 MiB; aggregate <=256 MiB.
SDR authority retains bounded PNG/JSON. HDR authority retains request and
attestation JSON/digests only. Raw readback, PPM, local diagnostics, and large
logs stay under ignored `Build/Validation/029/` or bounded CI retention. Reports
redact paths, credentials, native pointers, usernames, display serials, and
unstable marketing identifiers.
