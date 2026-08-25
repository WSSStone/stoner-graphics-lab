# Feature 028 CI Evidence

Final closeout records hosted regular, scheduled/manual medium, and required
Windows Vulkan plus physical M4 Pro Vulkan/Metal run IDs, revision IDs, artifact
names, and SHA-256 values here. Downloaded artifacts remain in ignored
`downloaded/`.

## Final Implementation Revision

The implementation authority is
`f92421502770139b5357420323a61e18ee112c76`. New hosted and physical
runs are triggered when this evidence draft is pushed. Hosted run `32803300825` on
predecessor `426d8617fe8558114110b09a60260de3895da82f` passed Windows, both macOS
lanes, ASan/UBSan, and TSan but failed Linux Lavapipe regular twice because its
first large native driver allocation occurred after the declared cycle-2 RSS
sample. The final revision primes both native paths once, releases every owner,
and preserves the exact 20/2 and 1000/20 contracts plus the 16 MiB limit.

Hosted run `32808481793` on code revision
`c82e51790643fc6583a80240239f6f4123cc0df1` proved the prime itself was
insufficient: Linux completed all 20 cycles with 40 captures, seven retained
readbacks, zero terminal owners, and stale-handle rejection, but released RSS
oscillated from 427,380,736 to 563,261,440 bytes and cycle 2 to cycle 20 grew
85,561,344 bytes. Revision `f92421502770139b5357420323a61e18ee112c76`
therefore introduced Linux/glibc heap trimming before the same
`/proc/self/statm` comparison samples; it did not change the metric, cycle
boundaries, or threshold. Manual medium run `32814481517` then showed that
trimming all 1,000 intermediate samples exhausted its remaining 1,467-second
native stage budget. The follow-up limits trimming to the authoritative warm-up
and terminal comparison cycles at code authority
`ffdc1a73994c8fb47971d8033628aba831af669d`, while preserving every intermediate
RSS sample and peak.

## Final Hosted Regular And Sanitizer Evidence

Push run `32811086369` passed on evidence revision
`2762019d69cec9a39a71467e82ada6331b7f5c4a`, whose direct code authority is
`f92421502770139b5357420323a61e18ee112c76`. Windows Vulkan, Linux Lavapipe
Vulkan, macOS arm64 Metal, macOS x86_64 Metal, ASan/UBSan, and TSan passed.
Four independent consumer jobs then downloaded and revalidated the immutable
regular artifacts successfully.

The corrected Linux run retained the exact 20-cycle / cycle-2 warm-up contract:
40 captures, seven final readbacks, zero terminal owners, stale-handle
rejection, 220,930,048 warm-up RSS bytes, 171,393,024 terminal RSS bytes,
323,387,392 peak RSS bytes, and zero positive growth. Its regular profile
finished in 325.050 seconds against the 600-second budget.

| Artifact | Size bytes | SHA-256 |
|---|---:|---|
| `production-regular-windows-vulkan-1` | 1,069,677,777 | `ac7874753f19971640123defa51e2a58e38547ef523c03c97512d3854ea6122e` |
| `production-regular-linux-vulkan-1` | 1,069,676,734 | `207ce85e3d121a0078caa17fab8ea983813c581448a544c01e96e8cefaf2ddb2` |
| `production-regular-macos-metal-1` | 1,070,847,441 | `3a4b1bda8f5b41c473b394d50e93198ce11d7b6b115050d346fbe8da889c6c5a` |
| `production-regular-macos-intel-metal-1` | 1,070,850,762 | `601d0ed00080e552dee4494dd7b19707c7dc6d79ffb3a6faa5c8df215458a80f` |
| `production-consumer-windows-vulkan-1` | 89,437 | `b0f0619472bcdb8486afe34257abc10a91d1df7ee325a5e70abbf46e959c9f9d` |
| `production-consumer-linux-vulkan-1` | 89,369 | `0bf66b5af0324807f4550734afb692966641bc4f838306fd065d4654d24ebf95` |
| `production-consumer-macos-metal-1` | 89,244 | `9f8941153603c8dd3916b68e8bd20908794a9681a7edc05f11e7ad18f98ba66d` |
| `production-consumer-macos-intel-metal-1` | 89,468 | `c9a81e68bac5de3177decbbce8397b901ccb606b6bac9daf233541ab65d136a0` |
| `production-sanitizer-asan-ubsan-1` | 1,909 | `f3fdb76c2602a5c45bc3c7637b67b31f7b083a4767f4c244c13a611a36132918` |
| `production-sanitizer-tsan-1` | 1,476 | `7b675959d58942c62e4730eea052dfd805bc5f9de964cf268e9bdc709c4c9e7d` |

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
