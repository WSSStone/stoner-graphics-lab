# Feature 028 Sanitizers And Failure Regressions

## Status

T110 remains complete on final code authority
`ffdc1a73994c8fb47971d8033628aba831af669d`. The macOS arm64 worktree passed
the focused malformed-input, cancellation, failure-injection, rollback,
shutdown, and lifetime regressions below. Final hosted run `32818269789` then
passed both required Linux ASan/UBSan and applicable TSan jobs on evidence head
`b7c89d6a5bbf92775db3b9f05af4d57e9bd5dc34`.

## Local Failure And Lifetime Gates

| Gate | Command | Result |
|---|---|---|
| Malformed glTF and bounded resolver/import rollback | `Build/Mac/Release/Tests/StonerTest --suite asset-gltf-malformed --suite asset-gltf-hardening` | PASS; 49-case malformed corpus plus resolver, limits, diagnostics, registry rollback, and mesh realization failure coverage |
| Cancellation, deadline, shutdown, and startup/shutdown races | `Build/Mac/Release/Tests/StonerTest --suite asset-manager-cancellation --suite asset-manager-shutdown` | PASS, 9 checks |
| Metal failure injection and Renderer realization rollback | `Build/Mac/Release/Tests/StonerTest --suite metal-failure-injection --suite renderer-static-model` | PASS; 9 named Metal failure points, zero terminal ownership, and 25 aggregate realization/rollback checks |
| Manager-independent handle lifetime and Feature 028 failure contracts | `Build/Mac/Release/Tests/StonerTest --suite asset-manager-lifetime --suite production-content` | PASS, 6 checks |

Metal failure evidence returned device, object, submission, and in-flight
ownership to zero. Renderer failures covered buffer, texture, shader, layout,
descriptor, sampler, pipeline, cancellation, and device-loss boundaries without
publishing a partial snapshot.

## Final-Revision Linux Gates

The final closeout ran at least:

```text
scons config=debug strict=1 sanitizers=address,undefined
Build/Linux/Debug/Tests/StonerTest --suite asset-gltf-malformed --suite asset-gltf-hardening --suite asset-manager-cancellation --suite asset-manager-shutdown --suite renderer-static-model --suite production-content

scons config=debug strict=1 sanitizers=thread
Build/Linux/Debug/Tests/StonerTest --suite asset-manager-cancellation --suite asset-manager-shutdown --suite asset-manager-concurrency --suite renderer-static-model --suite production-content
```

ASan/UBSan reported no memory or undefined-behavior diagnostic. TSan passed the
Asset Manager and CPU Renderer contract suites; native GPU/runtime lanes remain
owned by their non-TSan jobs. The uploaded artifacts are
`production-sanitizer-asan-ubsan-1` with SHA-256
`eaca0e84a383ad24d9a80474163b9c8efda7568600c9c022c2362846e83c018e` and
`production-sanitizer-tsan-1` with SHA-256
`aa23473ad6220c56a1b44008d5541737e2cf702e0bcf2207a22267a21d928bba`.
