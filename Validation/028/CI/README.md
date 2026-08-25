# Feature 028 CI Evidence

Final closeout records hosted regular, scheduled/manual medium, and required
Windows Vulkan plus physical M4 Pro Vulkan/Metal run IDs, revision IDs, artifact
names, and SHA-256 values here. Downloaded artifacts remain in ignored
`downloaded/`.

## Final Implementation Revision

The implementation authority is
`c82e51790643fc6583a80240239f6f4123cc0df1`. New hosted and physical runs are
triggered when the evidence draft is pushed. Hosted run `32803300825` on
predecessor `426d8617fe8558114110b09a60260de3895da82f` passed Windows, both macOS
lanes, ASan/UBSan, and TSan but failed Linux Lavapipe regular twice because its
first large native driver allocation occurred after the declared cycle-2 RSS
sample. The final revision primes both native paths once, releases every owner,
and preserves the exact 20/2 and 1000/20 contracts plus the 16 MiB limit.

## Final-Revision Local Metal Hardware Evidence

A detached clean worktree at predecessor
`426d8617fe8558114110b09a60260de3895da82f` passed strict
Debug and Release builds, acquired and verified the pinned corpus, and completed
the serialized physical M4 Pro Metal hardware profile.

| Field | Lantern v2 | Sponza v2 |
|---|---:|---:|
| Generation | `f60c3069294c588e146073d736dd8cd325760c8d1645ae6c412cf58c45a709d3` | `b39afe90356b699eed761aa548587883c9300d54568cb0eb94f279a09e84096b` |
| Reachable / reused | 37 / 37 | 189 / 189 |
| Lifecycle | 1,000 / warm-up 20 | 1,000 / warm-up 20 |
| Native seconds | 252.940 | 1,302.317 |
| Warm-up RSS bytes | 607,125,504 | 387,842,048 |
| Terminal RSS bytes | 524,943,360 | 391,462,912 |
| Peak RSS bytes | 608,681,984 | 398,475,264 |
| RSS growth bytes | 0 | 3,620,864 |
| Captures / readbacks | 2,000 / 7 | 2,000 / 7 |
| Semantic probes / FLIP | 20 / 0 | 20 / 0 |
| Terminal owners / stale | 0 / rejected | 0 / rejected |

The total profile time was 1,679.546 seconds against the schema-enforced
3,600-second budget. Independent verification accepted 4,369 artifacts. The
artifact-manifest SHA-256 is
`5cd36ce6a287e40884d77f538b838a5f06bce3edd23c76258a1b16cd3918bfdb`,
the summary SHA-256 is
`6c456e04daf300a02fc5e7668034094d5760c859ca795923059e1a22e72c1c83`,
and the target-profile SHA-256 is
`82681655db59befc20366978759ae42ace9ed805e2d4e0adc7de7980854f4c8f`.

## Local Vulkan And Medium Predecessor Evidence

The immediately preceding implementation revision
`f05a793780920b01dc7d711282f362cecb0ba803` passed the complete local physical
M4 Pro Vulkan hardware profile. The final delta only adds bounded
native-presentation recovery; final-binary 20-frame visible gates then passed
both Vulkan workloads with exact accepted baseline selection, 20 semantic
probes, FLIP 0, 40 captures, seven readbacks, zero terminal owners, and stale
handle rejection. The physical CI run above owns the same-revision 1,000-cycle
reconfirmation.

| Field | Lantern v2 | Sponza v2 |
|---|---:|---:|
| Generation | `88d01dc9de176f9f532ebeb1a3110f303a7b6067583260364485ff85e140091c` | `8fa033d770e58ea26574e840ada100fe174b05139575ef9de6a6557097c99073` |
| Lifecycle | 1,000 / warm-up 20 | 1,000 / warm-up 20 |
| Native seconds | 262.162 | 1,378.944 |
| Peak RSS bytes | 668,745,728 | 596,426,752 |
| RSS growth bytes | 0 | 7,012,352 |
| Terminal owners / stale | 0 / rejected | 0 / rejected |

That Vulkan producer emitted 4,368 artifacts with manifest SHA-256
`45a3cffd86d92f06f300c991e85091e5fe3d42b13d7a63a3f4514cf188257908`
and summary SHA-256
`457bdb2951f94d92533e145dc0988cf6f390c45ad0ddd26d2f049804f569addb`.

The same predecessor passed the clean-checkout Metal medium profile in
1,453.049 seconds. Lantern used zero post-warm-up RSS growth; Sponza used
10,469,376 bytes. Both completed 1,000 cycles, 100-percent warm reuse,
strict-no-source loading, semantic equivalence, zero terminal owners, and stale
handle rejection. Consumer verification accepted 4,289 artifacts with manifest
SHA-256
`6b97f099ec3203095ea345d18c31a09080df719129ccb3064da5e0b47b8d2d79`
and summary SHA-256
`b2d24c2ac74b4b97bbf23d8d80889114c0155e3f70f6384e3bcdfa08a9c2a75d`.

## Superseded Hosted Evidence

Hosted run `32711618360` passed the older revision
`0a5ad11f8d511a9b54da33da086b15cf530ca68a`, including all regular producers,
independent consumers, ASan/UBSan, and TSan. It is retained as regression
history but is not final closeout authority. Manual medium run `32716106974`
failed on that older revision and is likewise non-authoritative.
